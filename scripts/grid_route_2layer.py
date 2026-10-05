#!/usr/bin/env python3
"""Route using actual copper polygons, varying widths and native DRC rules."""
import sys,math,json,subprocess,re,time,struct,os
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
b=p.LoadBoard(str(ROOT/os.environ.get('GRID_INPUT','routing/priority.kicad_pcb')));apply_to_board(b)
original=p.LoadBoard(str(ROOT/'routing/priority.kicad_pcb'));fixedids={t.m_Uuid.AsString() for t in original.GetTracks()} | {t.m_Uuid.AsString() for t in b.GetTracks() if t.IsLocked()}
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
   holes.append((hole,pad.GetNetname() if pad.GetAttribute()!=p.PAD_ATTRIB_NPTH else None,pos,pad.m_Uuid.AsString()))
  if pad.GetAttribute()==p.PAD_ATTRIB_NPTH:continue
  for li,layer in enumerate(layers):
   if pad.IsOnLayer(layer):gs[li]=polyset(pad.GetEffectivePolygon(layer))
  if gs:
   rr=rec(pad.GetNetname(),gs,pad.m_Uuid.AsString(),'pad',pos)
   if pad.GetAttribute()==p.PAD_ATTRIB_SMD:smd.append(unary_union(list(gs.values())))
for t in b.GetTracks():
 if not isinstance(t,p.PCB_VIA):
  width=round(p.ToMM(t.GetWidth()),4)
  if t.m_Uuid.AsString() not in fixedids and classify(t.GetNetname()) in ['Data_Control','Default','Gate']:width=min(width,CLASSES[classify(t.GetNetname())][0])
  t.SetWidth(p.FromMM(width))
 net=t.GetNetname();uid=t.m_Uuid.AsString()
 if uid not in fixedids:newids.add(uid);history.setdefault(net,[]).append(uid)
 if isinstance(t,p.PCB_VIA):
  pos=xy(t.GetPosition());g=Point(pos).buffer(p.ToMM(t.GetWidth(p.F_Cu))/2,quad_segs=16);rec(net,{0:g,1:g},uid,'via',pos);holes.append((Point(pos).buffer(p.ToMM(t.GetDrillValue())/2,quad_segs=16),net,pos,uid))
 else:
  li=layers.index(t.GetLayer());ends=[xy(t.GetStart()),xy(t.GetEnd())];g=LineString(ends).buffer(p.ToMM(t.GetWidth())/2,quad_segs=8);rec(net,{li:g},uid)['ends']=ends
for z in b.Zones():
 if not z.GetIsRuleArea():
  for li,layer in enumerate(layers):
   if not z.IsOnLayer(layer):continue
   ps=z.GetFilledPolysList(layer)
   for oi in range(ps.OutlineCount()):
    o=ps.COutline(oi);pts=[xy(o.CPoint(j)) for j in range(o.PointCount())];hh=[]
    for hi in range(ps.HoleCount(oi)):
     hole=ps.CHole(oi,hi);hh.append([xy(hole.CPoint(j)) for j in range(hole.PointCount())])
    if len(pts)>2:rec(z.GetNetname(),{li:Polygon(pts,hh).buffer(0)},z.m_Uuid.AsString()+str(oi),'zone')
  continue
 g=polyset(z.Outline())
 for li,layer in enumerate(layers):
  if not z.IsOnLayer(layer):continue
  if z.GetDoNotAllowTracks():track_keep[li].append(g)
  if z.GetDoNotAllowVias():via_keep[li].append(g)
# Use a strict subset of the fine-pitch courtyards so emitted neckdowns satisfy native rules.
fine=[];powerfine=[]
for f in b.GetFootprints():
 if f.GetReference() not in ['U1','U3','U14','U15','U16','J1','U5','U9']:continue
 a=[r['gs'][0] for r in items if r['kind']=='pad' and r['uid'] in {pd.m_Uuid.AsString() for pd in f.Pads()} and 0 in r['gs']]
 if a:(powerfine if f.GetReference() in ['U5','U9'] else fine).append(polyset(f.GetCourtyard(p.F_Cu)).buffer(-.04,join_style=2))
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
powergrid=raster(powerfine).astype(bool)
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
def masks(net,src,tgt,soft=False):
 cls=classify(net);width,clear,vd,vdr=CLASSES[cls];widths=np.full((2,H,W),width,dtype=np.float32);widths[:,finegrid]=min(width,.15)
 if net in ['USB_VBUS','+3V2','+3V2_A','GND']:widths[:,powergrid]=min(width,.25)
 if net=='USB_VBUS':widths[0][raster([box(27.4,19.2,29.4,22.1)]).astype(bool)]=.25;widths[:,finegrid]=.15
 vdiam=np.full((H,W),vd,dtype=np.float32);drill=np.full((H,W),vdr,dtype=np.float32);vdiam[viafine]=.5;drill[viafine]=.2
 blocked=[];dists=[];penalties=[]
 for li in range(2):
  obs=[];softobs=[]
  for r in items:
   if r['uid'] in excluded or r['net']==net or li not in r['gs']:continue
   c=max(clear,r['clear']);key=(li,c)
   if key not in r['cache']:r['cache'][key]=r['gs'][li].buffer(c,quad_segs=8)
   (softobs if soft and r['uid'] in newids else obs).append(r['cache'][key])
  obs.extend(track_keep[li])
  obs.extend(h.buffer(.3) for h,hn,hp,hu in holes if hn is None and hu not in excluded)
  raw=raster(obs).astype(bool)|~edge
  dd=distance_transform_edt(~raw)*STEP;dists.append(dd)
  blocked.append((dd<widths[li]/2+.036).astype(np.uint8))
  sd=distance_transform_edt(~raster(softobs).astype(bool))*STEP if softobs else np.full((H,W),100.)
  penalties.append((sd<np.maximum(widths[li]/2+.035,vdiam/2+.035)).astype(np.float32)*40.)
 blocked=np.array(blocked,dtype=np.uint8)
 # Copper and drill separation for newly inserted vias, including all SMD lands.
 hop=(np.minimum(dists[0],dists[1])>=vdiam/2+.036)
 allhole=raster([h.buffer(.3) for h,hn,hp,hu in holes if hu not in excluded and (not soft or hu not in newids)]).astype(bool)
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
 start=np.stack([raster([r['gs'][li].buffer(-.04) for r in src if li in r['gs']]) for li in range(2)]) & ~blocked
 goal=np.stack([raster([r['gs'][li].buffer(-.04) for r in tgt if li in r['gs']]) for li in range(2)]) & ~blocked
 starts=np.flatnonzero(start.ravel()).astype(np.int32)
 if not len(starts) or not goal.any():return None,('no endpoints',len(starts),int(goal.sum()))
 heur=distance_transform_edt(~goal.any(axis=0)).astype(np.float32)
 inp=ROOT/'routing/grid-input.bin';out=ROOT/'routing/grid-path.txt'
 with inp.open('wb') as f:
  f.write(struct.pack('iii',W,H,len(starts)));f.write(starts.tobytes());f.write(blocked.tobytes());f.write(goal.astype(np.uint8).tobytes());f.write(hop.tobytes());f.write(heur.tobytes());f.write(np.array(penalties,dtype=np.float32).tobytes())
 proc=subprocess.run([str(ROOT/'routing/grid-route'),str(inp),str(out)],timeout=90)
 if proc.returncode:return None,('no path',out.read_text().strip())
 lines=out.read_text().splitlines();path=[tuple(map(int,l.split())) for l in lines[1:]]
 return (path,widths,vdiam,drill,hop),lines[0]
def emit(net,result):
 global counter
 path,widths,vdiam,drill,hop=result;routeids=[];data=[]
 def addseg(li,a,c,width):
  if a==c:return
  t=p.PCB_TRACK(b);t.SetStart(vv(a));t.SetEnd(vv(c));t.SetWidth(p.FromMM(round(float(width),4)));t.SetLayer(layers[li]);t.SetNet(b.GetNetsByName()[net]);b.Add(t);uid=t.m_Uuid.AsString();routeids.append(uid);newids.add(uid)
  rec(net,{li:LineString([a,c]).buffer(float(width)/2,quad_segs=8)},uid)['ends']=[a,c]
 i=0
 while i<len(path)-1:
  li,x,y=path[i];lj,xx,yy=path[i+1]
  if li!=lj:
   if hop[y,x]!=2:
    t=p.PCB_VIA(b);t.SetPosition(vv((x*STEP,y*STEP)));t.SetWidth(p.FromMM(round(float(vdiam[y,x]),4)));t.SetDrill(p.FromMM(round(float(drill[y,x]),4)));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(*layers);t.SetNet(b.GetNetsByName()[net]);t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t);uid=t.m_Uuid.AsString();routeids.append(uid);newids.add(uid);g=Point(x*STEP,y*STEP).buffer(float(vdiam[y,x])/2,quad_segs=16);rec(net,{0:g,1:g},uid,'via',(x*STEP,y*STEP));holes.append((Point(x*STEP,y*STEP).buffer(float(drill[y,x])/2,quad_segs=16),net,(x*STEP,y*STEP),uid))
   i+=1;continue
  width=min(float(widths[li,y,x]),float(widths[li,yy,xx]));direction=(xx-x,yy-y);j=i+1
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

def prune(net):
 removed=0
 for gr in groups(net):
  if not any(r['kind']=='pad' or r['uid'] in fixedids for r in gr):
   for r in gr:
    if r['uid'] in newids:excluded.add(r['uid']);removed+=1
 while True:
  rr=[r for r in items if r['net']==net and r['uid'] not in excluded];dead=set()
  trees={};layer_items={}
  for li in range(2):
   layer_items[li]=[r for r in rr if li in r['gs']];trees[li]=STRtree([r['gs'][li] for r in layer_items[li]])
  for r in rr:
   if r['uid'] not in newids:continue
   if r['kind']=='track':
    li=next(iter(r['gs']))
    for end in r.get('ends',[]):
     hit=trees[li].query(Point(end).buffer(.002),predicate='intersects')
     if not any(layer_items[li][j]['uid']!=r['uid'] for j in hit):dead.add(r['uid']);break
   elif r['kind']=='via' and net!='GND':
    connected=[]
    for li in range(2):
     hit=trees[li].query(r['gs'][li],predicate='intersects');connected.append(any(layer_items[li][j]['uid']!=r['uid'] for j in hit))
    if not all(connected):dead.add(r['uid'])
  if not dead:break
  excluded.update(dead);removed+=len(dead)
 if removed:print('PRUNED',net,removed,flush=True)

def priority(net):
 c=classify(net)
 order=['Clock_Source','Clock','Crystal','QSPI','Data_Control','USB_Pair','USB_Power','Sound_Power','Core_Power','Logic_Power','Analog_Power','Boost_Switch','CV_Analog','Audio','Line_Output','MIDI_Isolated','Gate','Default']
 return order.index(c) if c in order else 99
def blockers(net,result):
 path,widths,vdiam,drill,hop=result;gs=[[],[]];drills=[]
 for j,(li,x,y) in enumerate(path[:-1]):
  lj,xx,yy=path[j+1];pt=(x*STEP,y*STEP)
  if li!=lj:
   if hop[y,x]!=2:
    g=Point(pt).buffer(float(vdiam[y,x])/2+.025);gs[0].append(g);gs[1].append(g);drills.append(Point(pt).buffer(float(drill[y,x])/2+.30+.025))
  else:gs[li].append(LineString([pt,(xx*STEP,yy*STEP)]).buffer(float(widths[li,y,x])/2+.025))
 merged=[unary_union(g) for g in gs];dd=unary_union(drills);bad=set();clear=CLASSES[classify(net)][1]
 for r in items:
  if r['uid'] not in newids or r['uid'] in excluded or r['net']==net:continue
  if any(merged[li].intersects(g.buffer(max(clear,r['clear']))) for li,g in r['gs'].items()):bad.add(r['uid'])
 for h,hn,hp,hu in holes:
  if hu in newids and hu not in excluded and hn!=net and dd.intersects(h):bad.add(hu)
 return bad
nets=sorted({r['net'] for r in items if r['net'] and r['net']!='GND'},key=lambda n:(priority(n),n))
if os.environ.get('GRID_GND'):nets=['GND']
for candidate in nets:prune(candidate)
nets.sort(key=lambda net:(priority(net),1 if net in [f'D{i}' for i in range(8)] else 0,net))
queue=list(nets);fails=[];iterations=0;evictions={}
while queue and iterations<600:
 net=queue.pop(0);attempts=0;iterations+=1;prune(net)
 while True:
  gr=groups(net)
  if len(gr)<2:break
  geoms=[groupgeom(g) for g in gr];pairs=sorted((geoms[i].distance(geoms[j]),i,j) for i in range(len(gr)) for j in range(i+1,len(gr)))
  ok=False
  for distance,i,j in pairs[:6]:
   result,info=masks(net,gr[i],gr[j])
   if result is not None:emit(net,result);ok=True;print('CONNECTED',net,'groups',len(gr),'distance',round(distance,2),info,flush=True);break
  if not ok and net!='GND':
   for distance,i,j in pairs[:6]:
    result,info=masks(net,gr[i],gr[j],soft=True)
    if result is None:continue
    bad=blockers(net,result)
    excluded.update(bad);others={r['net'] for r in items if r['uid'] in bad}
    for other in others:
     prune(other)
     evictions[other]=evictions.get(other,0)+1
     if other not in queue:queue.append(other)
    emit(net,result);ok=True;print('RIPUP',net,'displaced',sorted(others),'objects',len(bad),info,flush=True);break
  if not ok:fails.append((net,len(gr),info));print('FAILED',net,len(gr),info,flush=True);break
  attempts+=1
  if counter%10==0:save()
  if attempts>60:fails.append((net,len(groups(net)),'attempt limit'));break
 if classify(net) in ['Clock_Source','Clock','QSPI','Data_Control'] and len(groups(net))==1:
  for t in b.GetTracks():
   if t.GetNetname()==net and t.m_Uuid.AsString() not in excluded:
    t.SetLocked(True);newids.discard(t.m_Uuid.AsString());fixedids.add(t.m_Uuid.AsString())
 save()
unresolved={net:len(groups(net)) for net in nets if len(groups(net))>1}
print('UNRESOLVED',unresolved,flush=True)
print('PASS DONE',counter,'connections; failures',fails,'remaining queue',queue,flush=True)
(ROOT/'routing/grid-failures.json').write_text(json.dumps({'failures':fails,'queue':queue,'evictions':evictions},indent=2)+'\n')
