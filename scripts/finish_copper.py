#!/usr/bin/python3
"""Ground pours and clearance-aware tented stitching, after signal routing."""
import math,sys
import pcbnew as p
from routing_rules import ROOT,apply_to_board,classify,CLASSES
q=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'routed/Chiptune_USB_CV.kicad_pcb');b=p.LoadBoard(str(q));apply_to_board(b)
def mm(x):return p.ToMM(x)
def v(x,y):return p.VECTOR2I(p.FromMM(x+100),p.FromMM(y+100))
def pt(q):return mm(q.x)-100,mm(q.y)-100
def line_dist(x,y,a,c):
 ax,ay=a;cx,cy=c;dx=cx-ax;dy=cy-ay;t=max(0,min(1,((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy))) if dx or dy else 0
 return math.hypot(x-ax-t*dx,y-ay-t*dy)
pads=[pad for f in b.GetFootprints() for pad in f.Pads()];tracks=list(b.GetTracks());mount=[(3,3,3),(82,3,3),(82,51,3),(3,51,3),(30.85,19.1,2.6),(54.15,19.1,2.6),(30.85,42.9,2.6),(54.15,42.9,2.6)]
def safe(x,y):
 if any(math.hypot(x-a,y-c)<r for a,c,r in mount):return False
 # Reserve oscillator backside as an uninterrupted reference plane.
 if 21.5<x<28.3 and 26.7<y<35.2:return False
 for pad in pads:
  cx,cy=pt(pad.GetPosition());a=math.radians(pad.GetOrientationDegrees());dx=(x-cx)*math.cos(a)-(y-cy)*math.sin(a);dy=(x-cx)*math.sin(a)+(y-cy)*math.cos(a)
  d=math.hypot(max(0,abs(dx)-mm(pad.GetSize().x)/2),max(0,abs(dy)-mm(pad.GetSize().y)/2))
  if d<max(.50,.35+CLASSES[classify(pad.GetNetname())][1]):return False
 for t in tracks:
  if isinstance(t,p.PCB_VIA):
   if math.hypot(x-pt(t.GetPosition())[0],y-pt(t.GetPosition())[1])<mm(t.GetWidth(p.F_Cu))/2+.5:return False
  elif t.GetNetname()!='GND':
   clear=CLASSES[classify(t.GetNetname())][1]
   if line_dist(x,y,pt(t.GetStart()),pt(t.GetEnd()))<mm(t.GetWidth())/2+.3+clear+.05:return False
 return True
added=0
for x in ([] if '--zones-only' in sys.argv else range(5,82,6)):
 for y in range(4,52,6):
  if not safe(x,y):continue
  t=p.PCB_VIA(b);t.SetPosition(v(x,y));t.SetWidth(p.FromMM(.6));t.SetDrill(p.FromMM(.3));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNet(b.GetNetsByName()['GND']);t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t);added+=1
for layer in [p.F_Cu,p.In2_Cu,p.B_Cu]:
 z=p.ZONE(b);z.SetLayer(layer);z.SetNet(b.GetNetsByName()['GND']);z.SetLocalClearance(p.FromMM(.20));z.SetMinThickness(p.FromMM(.20));z.SetPadConnection(p.ZONE_CONNECTION_THT_THERMAL);z.SetThermalReliefGap(p.FromMM(.25));z.SetThermalReliefSpokeWidth(p.FromMM(.25));z.SetIslandRemovalMode(p.ISLAND_REMOVAL_MODE_ALWAYS);o=z.Outline();o.NewOutline()
 for xy in [(.5,.5),(84.5,.5),(84.5,53.5),(.5,53.5)]:o.Append(v(*xy).x,v(*xy).y)
 b.Add(z)
b.BuildConnectivity();p.ZONE_FILLER(b).Fill(b.Zones());p.SaveBoard(str(q),b)
print('Ground pours filled; additional tented vias',added)
