#!/usr/bin/env python3
"""Route using actual copper polygons, varying widths and native DRC rules."""
import sys,math,json,subprocess,re,time,struct
from pathlib import Path
sys.path.append('/usr/lib/python3/dist-packages')
import pcbnew as p
import numpy as np
from scipy.ndimage import distance_transform_edt
from shapely.geometry import Point,LineString,Polygon,box
from shapely.affinity import rotate,translate
from shapely.ops import unary_union
from shapely.strtree import STRtree
from PIL import Image,ImageDraw
from routing_rules import ROOT,apply_to_board,classify,CLASSES
STEP=.05;W=1701;H=1081;AREA=W*H;layers=[p.F_Cu,p.B_Cu]
b=p.LoadBoard(str(ROOT/'routing/priority.kicad_pcb'));apply_to_board(b)
items=[];holes=[];smd=[];track_keep=[[],[]];via_keep=[[],[]];excluded=set();newids=set();history={};counter=0

def xy(v):return (p.ToMM(v.x)-100,p.ToMM(v.y)-100)
def vv(a):return p.VECTOR2I(p.FromMM(a[0]+100),p.FromMM(a[1]+100))
def polyset(ps):
 arr=[]
 for i in range(ps.OutlineCount()):
  o=ps.COutline(i);pts=[xy(o.CPoint(j)) for j in range(o.PointCount())];hh=[]
  for k in range(ps.HoleCount(i)):
   h=ps.CHole(i,k);hh.append([xy(h.CPoint(j)) for j in range(h.PointCount())])
  if len(pts)>2:arr.append(Polygon(pts,hh).buffer(0))
 return unary_union(arr) if arr else Polygon()
def rec(net,gs,uid,kind='track',centre=None):
 d={'net':net,'gs':gs,'uid':uid,'kind':kind,'centre':centre,'clear':CLASSES[classify(net)][1] if net else .15,'cache':{}};items.append(d);return d
for f in b.GetFootprints():
 for pad in f.Pads():
  gs={};pos=xy(pad.GetPosition());dr=pad.GetDrillSize();dx=p.ToMM(dr.x);dy=p.ToMM(dr.y)
  if dx and dy:
   if abs(dx-dy)<1e-6:hole=Point(pos).buffer(dx/2,quad_segs=16)
   else:
    r=min(dx,dy)/2;l=abs(dx-dy)/2;hole=LineString([(-l,0),(l,0)] if dx>dy else [(0,-l),(0,l)]).buffer(r,quad_segs=16);hole=translate(rotate(hole,-pad.GetOrientationDegrees(),origin=(0,0)),*pos)
   holes.append((hole,pad.GetNetname() if pad.GetAttribute()!=p.PAD_ATTRIB_NPTH else None,pos))
  if pad.GetAttribute()==p.PAD_ATTRIB_NPTH:continue
  for li,layer in enumerate(layers):
   if pad.IsOnLayer(layer):gs[li]=polyset(pad.GetEffectivePolygon(layer))
  if gs:
   rr=rec(pad.GetNetname(),gs,pad.m_Uuid.AsString(),'pad',pos)
   if pad.GetAttribute()==p.PAD_ATTRIB_SMD:smd.append(unary_union(list(gs.values())))
for t in b.GetTracks():
 net=t.GetNetname();uid=t.m_Uuid.AsString()
 if isinstance(t,p.PCB_VIA):
  pos=xy(t.GetPosition());g=Point(pos).buffer(p.ToMM(t.GetWidth(p.F_Cu))/2,quad_segs=16);rec(net,{0:g,1:g},uid,'via',pos);holes.append((Point(pos).buffer(p.ToMM(t.GetDrillValue())/2,quad_segs=16),net,pos))
 else:
  li=layers.index(t.GetLayer());g=LineString([xy(t.GetStart()),xy(t.GetEnd())]).buffer(p.ToMM(t.GetWidth())/2,quad_segs=8);rec(net,{li:g},uid)
for z in b.Zones():
 if not z.GetIsRuleArea():continue
 g=polyset(z.Outline())
 for li,layer in enumerate(layers):
  if not z.IsOnLayer(layer):continue
  if z.GetDoNotAllowTracks():track_keep[li].append(g)
  if z.GetDoNotAllowVias():via_keep[li].append(g)
# Use a strict subset of the fine-pitch courtyards so emitted neckdowns satisfy native rules.
fine=[]
for f in b.GetFootprints():
 if f.GetReference() not in ['U1','U3','U14','U15','U16','J1']:continue
 a=[r['gs'][0] for r in items if r['kind']=='pad' and r['uid'] in {pd.m_Uuid.AsString() for pd in f.Pads()} and 0 in r['gs']]
 if a:fine.append(box(*unary_union(a).bounds).buffer(.10,join_style=2))
finegeom=unary_union(fine)
boardgeom=box(3,3,82,51).buffer(3,quad_segs=24)

def drawgeom(im,g,val=1):
 if g.is_empty:return
 if g.geom_type in ['MultiPolygon','GeometryCollection']:
  for sub in g.geoms:drawgeom(im,sub,val)
  return
 if g.geom_type!='Polygon':return
 dr=ImageDraw.Draw(im);dr.polygon([(round(x/STEP),round(y/STEP)) for x,y in g.exterior.coords],fill=val)
 for h in g.interiors:dr.polygon([(round(x/STEP),round(y/STEP)) for x,y in h.coords],fill=0)
def raster(gs):
 im=Image.new('L',(W,H),0)
 for g in gs:drawgeom(im,g)
 return np.asarray(im,dtype=np.uint8).copy()
finegrid=raster([finegeom]).astype(bool);viafine=raster([finegeom.buffer(2)]).astype(bool)
# Raster boundary distances include the half-cell diagonal as a geometric margin.
edge=raster([boardgeom.buffer(-.5)]).astype(bool)

class UF:
 def __init__(self,n):self.p=list(range(n))
 def root(self,i):
  while self.p[i]!=i:self.p[i]=self.p[self.p[i]];i=self.p[i]
  return i
 def join(self,a,c):self.p[self.root(a)]=self.root(c)
def groups(net):
 rr=[r for r in items if r['net']==net and r['uid'] not in excluded];uf=UF(len(rr))
 for li in range(2):
  ii=[i for i,r in enumerate(rr) if li in r['gs']];gs=[rr[i]['gs'][li] for i in ii]
  if not gs:continue
  tree=STRtree(gs)
  for j,g in enumerate(gs):
   for k in tree.query(g,predicate='intersects'):
    if k>j:uf.join(ii[j],ii[k])
 out={}
 for i,r in enumerate(rr):out.setdefault(uf.root(i),[]).append(r)
 return list(out.values())
def groupgeom(gr):return unary_union([g for r in gr for g in r['gs'].values()])
def masks(net,src,tgt):
 cls=classify(net);width,clear,vd,vdr=CLASSES[cls];widths=np.full((2,H,W),width,dtype=np.float32);widths[0][finegrid]=min(width,.15)
 if net=='USB_VBUS':widths[0][raster([box(27.4,19.2,29.4,22.1)]).astype(bool)]=.25;widths[0][finegrid]=.15
 vdiam=np.full((H,W),vd,dtype=np.float32);drill=np.full((H,W),vdr,dtype=np.float32);vdiam[viafine]=.5;drill[viafine]=.2
 blocked=[];dists=[]
 for li in range(2):
  obs=[]
  for r in items:
   if r['uid'] in excluded or r['net']==net or li not in r['gs']:continue
   c=max(clear,r['clear']);key=(li,c)
   if key not in r['cache']:r['cache'][key]=r['gs'][li].buffer(c,quad_segs=8)
   obs.append(r['cache'][key])
  obs.extend(track_keep[li])
  obs.extend(h.buffer(.3) for h,hn,hp in holes if hn is None)
  raw=raster(obs).astype(bool)|~edge
  dd=distance_transform_edt(~raw)*STEP;dists.append(dd)
  blocked.append((dd<widths[li]/2+.036).astype(np.uint8))
 blocked=np.array(blocked,dtype=np.uint8)
 # Copper and drill separation for newly inserted vias, including all SMD lands.
 hop=(np.minimum(dists[0],dists[1])>=vdiam/2+.036)
 allhole=raster([h.buffer(.3) for h,hn,hp in holes]).astype(bool)
 hole_dist=distance_transform_edt(~allhole)*STEP;hop&=hole_dist>=drill/2+.036
 land=raster(smd).astype(bool);land_dist=distance_transform_edt(~land)*STEP
 hop&=land_dist>=np.maximum(vdiam/2,drill/2+.15)+.036
 keep=raster(via_keep[0]+via_keep[1]).astype(bool);keep_dist=distance_transform_edt(~keep)*STEP
 hop&=keep_dist>=vdiam/2+.036
 hop=hop.astype(np.uint8)
 # Existing plated holes connect layers without adding another drilled hole.
 for r in items:
  if r['net']==net and r['uid'] not in excluded and 0 in r['gs'] and 1 in r['gs'] and r['centre']:
   x,y=r['centre'];ix=round(x/STEP);iy=round(y/STEP)
   if 0<=ix<W and 0<=iy<H and not blocked[0,iy,ix] and not blocked[1,iy,ix]:hop[iy,ix]=2
 start=np.stack([raster([r['gs'][li] for r in src if li in r['gs']]) for li in range(2)]) & ~blocked
 goal=np.stack([raster([r['gs'][li] for r in tgt if li in r['gs']]) for li in range(2)]) & ~blocked
 starts=np.flatnonzero(start.ravel()).astype(np.int32)
 if not len(starts) or not goal.any():return None,('no endpoints',len(starts),int(goal.sum()))
 heur=distance_transform_edt(~goal.any(axis=0)).astype(np.float32)
 inp=ROOT/'routing/grid-input.bin';out=ROOT/'routing/grid-path.txt'
 with inp.open('wb') as f:
  f.write(struct.pack('iii',W,H,len(starts)));f.write(starts.tobytes());f.write(blocked.tobytes());f.write(goal.astype(np.uint8).tobytes());f.write(hop.tobytes());f.write(heur.tobytes())
 proc=subprocess.run([str(ROOT/'routing/grid-route'),str(inp),str(out)],timeout=90)
 if proc.returncode:return None,('no path',out.read_text().strip())
 lines=out.read_text().splitlines();path=[tuple(map(int,l.split())) for l in lines[1:]]
 return (path,widths,vdiam,drill,hop),lines[0]
def emit(net,result):
 global counter
 path,widths,vdiam,drill,hop=result;routeids=[];data=[]
 def addseg(li,a,c,width):
  if a==c:return
  t=p.PCB_TRACK(b);t.SetStart(vv(a));t.SetEnd(vv(c));t.SetWidth(p.FromMM(float(width)));t.SetLayer(layers[li]);t.SetNet(b.GetNetsByName()[net]);b.Add(t);uid=t.m_Uuid.AsString();routeids.append(uid);newids.add(uid)
  rec(net,{li:LineString([a,c]).buffer(float(width)/2,quad_segs=8)},uid)
 i=0
 while i<len(path)-1:
  li,x,y=path[i];lj,xx,yy=path[i+1]
  if li!=lj:
   if hop[y,x]!=2:
    t=p.PCB_VIA(b);t.SetPosition(vv((x*STEP,y*STEP)));t.SetWidth(p.FromMM(float(vdiam[y,x])));t.SetDrill(p.FromMM(float(drill[y,x])));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(*layers);t.SetNet(b.GetNetsByName()[net]);t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t);uid=t.m_Uuid.AsString();routeids.append(uid);newids.add(uid);g=Point(x*STEP,y*STEP).buffer(float(vdiam[y,x])/2,quad_segs=16);rec(net,{0:g,1:g},uid,'via',(x*STEP,y*STEP));holes.append((Point(x*STEP,y*STEP).buffer(float(drill[y,x])/2,quad_segs=16),net,(x*STEP,y*STEP)))
   i+=1;continue
  width=float(widths[li,y,x]);direction=(xx-x,yy-y);j=i+1
  while j+1<len(path):
   lk,xxx,yyy=path[j+1];cl,cx,cy=path[j]
   if lk!=li or (xxx-cx,yyy-cy)!=direction or abs(float(widths[li,yyy,xxx])-width)>.001:break
   j+=1
  addseg(li,(x*STEP,y*STEP),(path[j][1]*STEP,path[j][2]*STEP),width);i=j
 history.setdefault(net,[]).extend(routeids);counter+=1

def save():
 q=ROOT/'routing/grid.kicad_pcb';p.SaveBoard(str(q),b)
 if excluded:
  s=q.read_text()
  def filt(m):
   uu=re.search(r'\(uuid "([^"]+)"\)',m[0]);return '' if uu and uu[1] in excluded else m[0]
  s=re.sub(r'^\t\((?:segment|via)\n.*?^\t\)\n',filt,s,flags=re.M|re.S);q.write_text(s)

def priority(net):
 c=classify(net)
 order=['USB_Power','Sound_Power','Core_Power','Logic_Power','Analog_Power','Boost_Switch','Clock','Crystal','QSPI','CV_Analog','Audio','Line_Output','MIDI_Isolated','Data_Control','Gate','Default','USB_Pair']
 return order.index(c) if c in order else 99
nets=sorted({r['net'] for r in items if r['net'] and r['net']!='GND'},key=lambda n:(priority(n),n));fails=[]
for net in nets:
 attempts=0
 while True:
  gr=groups(net)
  if len(gr)<2:break
  geoms=[groupgeom(g) for g in gr];pairs=sorted((geoms[i].distance(geoms[j]),i,j) for i in range(len(gr)) for j in range(i+1,len(gr)))
  ok=False
  for distance,i,j in pairs[:6]:
   result,info=masks(net,gr[i],gr[j])
   if result is not None:emit(net,result);ok=True;print('CONNECTED',net,'groups',len(gr),'distance',round(distance,2),info,flush=True);break
  if not ok:fails.append((net,len(gr),info));print('FAILED',net,len(gr),info,flush=True);break
  attempts+=1
  if counter%10==0:save()
  if attempts>40:print('Too many attempts',net,flush=True);break
 save()
print('FIRST PASS DONE',counter,'connections; failures',fails,flush=True)
(ROOT/'routing/grid-failures.json').write_text(json.dumps(fails,indent=2)+'\n')
