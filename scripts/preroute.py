#!/usr/bin/python3
"""Short local boost and decoupling routes, kept before geometric autorouting."""
import pcbnew as p
B='hardware/Chiptune_2AAA.kicad_pcb';b=p.LoadBoard(B)
def v(x,y):return p.VECTOR2I(p.FromMM(100+x),p.FromMM(100+y))
nets=b.GetNetsByName()
def trace(net,xy,width,layer=p.F_Cu):
 for a,z in zip(xy,xy[1:]):
  t=p.PCB_TRACK(b);t.SetStart(v(*a));t.SetEnd(v(*z));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(nets[net]);b.Add(t)
def via(net,x,y):
 a=p.PCB_VIA(b);a.SetPosition(v(x,y));a.SetWidth(p.FromMM(.6));a.SetDrill(p.FromMM(.3));a.SetViaType(p.VIATYPE_THROUGH);a.SetLayerPair(p.F_Cu,p.B_Cu);a.SetNet(nets[net]);b.Add(a)
trace('BOOST_SW',[(21.7125,43),(22.6,43)],.25)
trace('BOOST_SW',[(22.6,43),(24.8,43),(25.5,42.75)],.8)
trace('BAT_PROT',[(25.5,35.45),(25.5,38.25)],1.0)
trace('BAT_PROT',[(20.2875,43.5),(19.5,43.5)],.25)
trace('BAT_PROT',[(19.5,43.5),(19.05,43.95),(16,43.95)],1.0)
trace('+5V',[(21.7125,42.5),(21.7125,41.5)],.25)
trace('+5V',[(21.7125,41.5),(21.7125,41.1625),(20.5,39.95),(18,39.95)],.6)
trace('GND',[(21.7125,43.5),(22.5,44.15)],.25);via('GND',22.5,44.15)
for x,y,dx,dy in [(25.5,33.55,25.5,32.4),(16,42.05,15,42.05),(18,38.05,18,37.0),(20.5,38.05,20.5,37.0)]:
 trace('GND',[(x,y),(dx,dy)],.4);via('GND',dx,dy)
# EP grounding vias inside the RP2040 pad; tented on both masks.
for x,y in [(24.3,16.3),(25.7,16.3),(24.3,17.7),(25.7,17.7)]:via('GND',x,y)
# Join the two AAA cells with a thick underside trace.
trace('BAT_MID',[(67.875,35),(68.9,35),(68.9,46),(67.875,46)],1.5,p.B_Cu)
# Protected battery distribution, bottom side to reduce series resistance.
for x,y in [(16,43.95),(25.5,35.45)]:via('BAT_PROT',x,y)
trace('BAT_PROT',[(16,43.95),(25.5,34.45),(25.5,35.45)],1.3,p.B_Cu)
# Same-net paths terminate at the source/drain pads through a short neck.
fp={f.GetReference():f for f in b.GetFootprints()};pads={d.GetNumber():d for d in fp['Q1'].Pads()}
for num in ['2','3']:
 d=pads[num];pos=d.GetPosition();x=p.ToMM(pos.x)-100;y=p.ToMM(pos.y)-100
 via(d.GetNetname(),x-1.0,y);trace(d.GetNetname(),[(x,y),(x-1.0,y)],.4)
 if num=='2':trace('BAT_PROT',[(x-1.0,y),(16,43.95)],1.3,p.B_Cu)
 else:trace('BAT_RAW',[(12.125,35),(12.125,38.8),(x-1.0,y)],1.3,p.B_Cu)
p.SaveBoard(B,b)
