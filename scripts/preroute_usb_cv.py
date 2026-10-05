#!/usr/bin/python3
"""Critical nets first. All dimensions in millimetres, source layout preserved."""
from pathlib import Path
import shutil
import pcbnew as p
from routing_rules import ROOT,apply_to_board
b=p.LoadBoard(str(ROOT/'hardware/Chiptune_USB_CV.kicad_pcb'));apply_to_board(b)
ns=b.GetNetsByName()
def v(x,y):return p.VECTOR2I(p.FromMM(x+100),p.FromMM(y+100))
def route(net,points,width=.25,layer=p.F_Cu):
 for a,c in zip(points,points[1:]):
  if a==c:continue
  t=p.PCB_TRACK(b);t.SetStart(v(*a));t.SetEnd(v(*c));t.SetWidth(p.FromMM(width));t.SetLayer(layer);t.SetNet(ns[net]);t.SetLocked(True);b.Add(t)
def via(net,xy,size=.6,drill=.3):
 t=p.PCB_VIA(b);t.SetPosition(v(*xy));t.SetWidth(p.FromMM(size));t.SetDrill(p.FromMM(drill));t.SetViaType(p.VIATYPE_THROUGH);t.SetLayerPair(p.F_Cu,p.B_Cu);t.SetNet(ns[net]);t.SetLocked(True);t.SetFrontTentingMode(p.TENTING_MODE_TENTED);t.SetBackTentingMode(p.TENTING_MODE_TENTED);b.Add(t)
def keepout(points,layer,tracks=False):
 z=p.ZONE(b);z.SetIsRuleArea(True);z.SetLayer(layer);z.SetDoNotAllowVias(True);z.SetDoNotAllowTracks(tracks);z.SetDoNotAllowPads(False);z.SetDoNotAllowCopperPour(False);o=z.Outline();o.NewOutline()
 for xy in points:o.Append(int(v(*xy).x),int(v(*xy).y))
 b.Add(z)
# USB-C duplicate contacts, ESD flow-through, series resistors, MCU.
P='USB_D_P';N='USB_D_N'
route(P,[(33.75,46.305),(33.75,45.1)],.15);via(P,(33.75,45.1))
route(P,[(34.75,46.305),(34.75,48.3)],.15);via(P,(34.75,48.3))
route(P,[(34.75,48.3),(35.35,48.3)],layer=p.B_Cu)
route(P,[(33.75,45.1),(33.75,45.6),(34.1,45.95),(34.4,45.95),(35.35,45)],layer=p.B_Cu)
route(P,[(35.35,45),(35.35,49),(36,49),(36,45.3),(39.3,45.3),(39.2,45.4),(39.2,46.0)],layer=p.B_Cu);via(P,(39.2,46.0))
route(P,[(39.2,46.0),(39.2,46.8875),(40.3625,48.05)])
route(N,[(34.25,46.305),(34.25,45.4),(34.5,45.15),(34.5,44.85)],.15);via(N,(34.5,44.85))
route(N,[(33.25,46.305),(33.25,45.5),(33,45.25),(33,45)],.15);via(N,(33,45))
route(N,[(33,45),(33,43.7),(34.5,43.7),(34.5,44.85)],layer=p.B_Cu)
route(N,[(34.5,44.85),(34.5,43.65),(35.65,43.65),(36.9,44.9),(39.4657,44.9),(39.8,45.2343),(39.8,48.1),(39,48.9)],layer=p.B_Cu);via(N,(39,48.9))
route(N,[(39,48.9),(39,49.8),(39.15,49.95),(40.3625,49.95)])
route(P,[(40.3625,48.05),(42.6375,48.05)])
route(N,[(40.3625,49.95),(42.6375,49.95)])
route(P,[(42.6375,48.05),(43,48.05),(43.2,47.85),(43.2,46.5)]);via(P,(43.2,46.5))
route(P,[(43.2,46.5),(43.2,45.5),(43.5,45.2),(43.5,37.0828),(41.2172,34.8),(36.3,34.8)],layer=p.B_Cu);via(P,(36.3,34.8))
route(P,[(36.3,34.8),(36.3,34.525),(37,33.825)])
route(N,[(42.6375,49.95),(43,49.95),(44.2,48.75),(44.2,45.8)]);via(N,(44.2,45.8))
route(N,[(44.2,45.8),(44.2,45.1),(43.9,44.8),(43.9,36.9172),(41.3828,34.4),(40.4,34.4),(40,34)],layer=p.B_Cu);via(N,(40,34))
route(N,[(40,34),(39.825,33.825),(39,33.825)])
route('USB_MCU_D_P',[(37,32.175),(36.425,32.175),(36.25,32),(35.4375,32)],.25)
route('USB_MCU_D_N',[(39,32.175),(39,32.65),(38.65,33),(36.6,33),(36,32.4),(35.4375,32.4)],.25)
# Short ESD VBUS bias branch escapes between its data pads.
route('USB_VBUS',[(42.6375,49),(41.6,49)],.25);via('USB_VBUS',(41.6,49),.6,.3)
# Shared bus: parallel rear-layer rails, front-layer socket breakouts.
# SAA1099 pin10..17 are deliberately D7..D0; reverse every firmware write.
for bit,y in [(3,3.2),(4,2.65),(5,2.1),(6,1.55),(7,1.0)]:
 net=f'D{bit}';xs=[10.04+2.54*bit,{3:36.54,4:41.62,5:44.16,6:46.7,7:49.24}[bit],{3:61.04,4:66.12,5:68.66,6:71.2,7:73.74}[bit]]
 for x in xs:
  route(net,[(x,4.68),(x,y)],.15);via(net,(x,y),.5,.2)
 route(net,[(xs[0],y),(xs[2],y)],.15,p.B_Cu)
for bit,y in [(0,14.9),(1,15.45),(2,16.0)]:
 net=f'D{bit}';x=10.04+2.54*bit;escape=x+1.27
 if bit==0:
  escape=11.0;points=[(x,4.68),(11.31,5.95),(11.31,13.4),(11.0,13.71),(11.0,y)]
 elif bit==2:
  escape=14.75;points=[(x,4.68),(16.39,5.95),(16.39,13.4),(14.75,15.04),(14.75,y)]
 else:points=[(x,4.68),(escape,5.95),(escape,y)]
 xs=[escape,{0:39.08,1:36.54,2:35.4}[bit],{0:63.58,1:61.04,2:58.5}[bit]]
 route(net,points,.15);via(net,(escape,y),.5,.2)
 for k,x in enumerate(xs[1:]):
  points=[(x,12.3),(x,y)]
  if bit==2 and k==0:points=[(34,12.3),(34,12.8),(35.4,14.2),(35.4,y)]
  route(net,points,.15);via(net,(x,y),.5,.2)
 route(net,[(xs[0],y),(xs[2],y)],.15,p.B_Cu)
# Common SN clock follows the socket centres on rear copper.
for x in [39.08,63.58]:
 route('SN_CLK',[(x,4.68),(x,9)],.20);via('SN_CLK',(x,9))
route('SN_CLK',[(39.08,9),(63.58,9)],.20,p.B_Cu)
# Common write strobe: rear rail through the socket centres.
for x in [7.5,44.16,68.66]:
 route('WR_N',[(x,12.3),(x,10.2)],.15);via('WR_N',(x,10.2),.5,.2)
route('WR_N',[(7.5,10.2),(68.66,10.2)],.15,p.B_Cu)
# Crystal paths are entirely on front copper, without signal vias.
route('XIN',[(28.5625,30.4),(27.6,30.4),(26.65,29.45),(26.65,29),(26.45,28.8),(23.75,28.8),(22.65,29.9)],.15)
route('XIN',[(22.65,29.9),(22.65,28.625),(23.25,28.025)],.15)
route('XOUT',[(28.5625,30.8),(27.25,30.8),(27.125,30.675),(26.5,30.675)],.15)
route('XTAL_OUT',[(26.5,32.325),(26.275,32.1),(24.35,32.1)],.15)
route('XTAL_OUT',[(24.35,32.1),(24.35,34.375),(24.225,34.5)],.15)
for layer in [p.B_Cu,p.In1_Cu]:
 keepout([(21.8,27),(25.5,27),(25.5,34.9),(21.8,34.9)],layer,True)
 keepout([(25.5,29.9),(27.1,29.9),(27.4,30.2),(28,30.2),(28,31.4),(25.5,31.4)],layer,True)
# Short converter switching loop, input/output capacitors, quiet FB divider.
route('BOOST_SW',[(26.5375,20.5),(25.7,20.5)],.25)
route('BOOST_SW',[(25.7,20.5),(24.75,20.5)],.75)
route('+5V',[(26.5375,21),(26.5375,21.65)],.25)
route('+5V',[(26.5375,21.65),(27.25,22.3625),(27.25,22.55)],.75)
route('GND',[(27.25,24.45),(26,23.2),(26,22.9)],.5);via('GND',(26,22.9))
route('GND',[(26.5375,20),(26.5375,19.2375)],.25)
route('GND',[(26.5375,19.2375),(25.8,18.5)],.5);via('GND',(25.8,18.5))
route('GND',[(26,22.9),(25.8,22.7),(25.8,18.5)],.5,p.B_Cu)
route('GND',[(17.5,21.725),(16.25,21.725)],.5);via('GND',(16.25,21.725))
route('GND',[(16.25,21.725),(19.475,18.5),(25.8,18.5)],.75,p.B_Cu)
route('USB_VBUS',[(17.5,23.275),(18.3,23.275),(20.25,21.325),(20.25,20.5)],1)
via('USB_VBUS',(18.75,22.825),.8,.4)
route('USB_VBUS',[(27.9625,20),(28.45,20),(28.9,20.45),(28.9,21.5)],.25);via('USB_VBUS',(28.9,21.5))
route('USB_VBUS',[(18.75,22.825),(20.175,24.25),(26.15,24.25),(28.9,21.5)],1,p.B_Cu)
route('BOOST_FB',[(27.25,18.325),(27.25,19.5)],.2);via('BOOST_FB',(27.25,19.5),.5,.2)
route('BOOST_FB',[(27.9625,21),(28.25,21),(28.25,22.45),(28.5,22.7),(29.225,22.7),(29.925,22)],.2)
route('BOOST_FB',[(29.225,22.7),(29.25,22.675),(29.25,22.5)],.2);via('BOOST_FB',(29.25,22.5),.5,.2)
route('BOOST_FB',[(27.25,19.5),(28.5,18.25),(29,18.25),(29,17.325),(32,17.325),(33,18.325),(33,19.5),(30,22.5),(29.25,22.5)],.2,p.B_Cu)
route('SOUND_ENABLE',[(27.9625,20.5),(27.3,20.5)],.15);via('SOUND_ENABLE',(27.3,20.5),.5,.2)
z=p.ZONE(b);z.SetIsRuleArea(True);z.SetZoneName('BOOST_VIN_BRANCH');z.SetLayer(p.F_Cu);z.SetDoNotAllowVias(False);z.SetDoNotAllowTracks(False);z.SetDoNotAllowPads(False);z.SetDoNotAllowCopperPour(False);o=z.Outline();o.NewOutline()
for xy in [(27.4,19.2),(29.4,19.2),(29.4,22.1),(27.4,22.1)]:o.Append(int(v(*xy).x),int(v(*xy).y))
b.Add(z)
# RP2040 ground paddle: drilled vias outside the paste land, connected by straps.
for x in [29.9,34.1]:
 for y in [30.2,31.7]:
  via('GND',(x,y));route('GND',[(32,y),(x,y)],.5)
route('GND',[(28.5625,30),(29.7,30),(29.9,30.2)],.15)
# RP2040 power escapes: short 0.15 mm leads to vias, wider distribution on back.
for net,xy,points in [
 ('+3V2',(35.55,26.55),[(34.6,27.5625),(34.6,26.9),(34.95,26.55),(35.55,26.55)]),
 ('+3V2',(31,28.8),[(31,27.5625),(31,28.8)]),
 ('+3V2',(29.5,31),[(28.5625,31.2),(29.3,31.2),(29.5,31)]),
 ('+3V2',(31.2,33.02),[(31,34.4375),(31,33.8),(31.2,33.6),(31.2,33.02)]),
 ('+3V2',(34.2,33.5),[(34.6,34.4375),(34.6,33.9),(34.2,33.5)]),
 ('+3V2',(37,31),[(35.4375,31.2),(36.8,31.2),(37,31)]),
 ('+1V1',(29.5,32.75),[(28.5625,31.6),(29.25,31.6),(29.25,32.5),(29.5,32.75)]),
 ('+1V1',(34.1,32.65),[(35.4375,32.8),(34.25,32.8),(34.1,32.65)]),
 ('+1V1',(34.5,30.95),[(35.4375,30.8),(34.65,30.8),(34.5,30.95)]),
 ('+3V2_A',(35.55,34.4),[(35.4375,33.6),(35.55,33.6),(35.55,34.4)]),
]:
 route(net,points,.15);via(net,xy,.5,.2)
# Widen the two segments beyond the package courtyard.
for t in b.GetTracks():
 if isinstance(t,p.PCB_VIA) or t.GetNetname()!='+3V2':continue
 if t.GetStart()==v(34.95,26.55) or t.GetStart()==v(36.8,31.2):t.SetWidth(p.FromMM(.45))
route('+3V2',[(35.4375,31.6),(35.95,31.6),(35.95,31.2)],.15)
route('+3V2',[(35.4375,33.2),(34.2,33.2),(34.2,33.5)],.15)

# Fine vias at RP2040: 0.50/0.20 mm, 0.15 mm annular copper.
for net,x,y,points in [
 ('D7',34.2,28.65,[(34.2,27.5625),(34.2,28.65)]),
 ('D6',33.8,26.5,[(33.8,27.5625),(33.8,26.5)]),
 ('D5',33.4,28.65,[(33.4,27.5625),(33.4,28.65)]),
 ('D4',33,26.5,[(33,27.5625),(33,26.5)]),
 ('D3',32.6,28.65,[(32.6,27.5625),(32.6,28.65)]),
 ('D2',32.2,26.5,[(32.2,27.5625),(32.2,26.5)]),
 ('D1',31.8,28.65,[(31.8,27.5625),(31.8,28.65)]),
 ('D0',31.4,26.5,[(31.4,27.5625),(31.4,26.5)]),
 ('WR_N',30.6,26.5,[(30.6,27.5625),(30.6,26.5)]),
 ('SAA_CS_N',30.2,28.65,[(30.2,27.5625),(30.2,28.65)]),
 ('SN1_CS_N',29.8,26.5,[(29.8,27.5625),(29.8,26.5)]),
 ('SN2_CS_N',29.5,29.2,[(29.4,27.5625),(29.4,28.6),(29.5,28.7),(29.5,29.2)]),
 ('FLASH_CS',37,26.9,[(35.4375,28.4),(35.95,28.4),(35.95,27.45),(36.5,26.9),(37,26.9)]),
 ('FLASH_D1',37.6,27.4,[(35.4375,28.8),(36.3,28.8),(36.3,27.9),(36.8,27.4),(37.6,27.4)]),
 ('FLASH_D2',37,28,[(35.4375,29.2),(36.65,29.2),(36.65,28.35),(37,28)]),
 ('FLASH_D0',37.55,28.8,[(35.4375,29.6),(36.75,29.6),(37.55,28.8)]),
 ('FLASH_CLK',37.5,30,[(35.4375,30),(37.5,30)]),
 ('FLASH_D3',36.35,30.55,[(35.4375,30.4),(36.2,30.4),(36.35,30.55)]),
 ('UI_SDA',28.5,35.1,[(29.4,34.4375),(29.4,34.75),(29.05,35.1),(28.5,35.1)]),
 ('UI_SCL',30.2,36.3,[(29.8,34.4375),(29.8,35),(30.2,35.4),(30.2,36.3)]),
 ('UI_MINUS',29.85,33.6,[(30.2,34.4375),(30.2,33.9),(29.9,33.6),(29.85,33.6)]),
 ('UI_OK',30.2,33,[(30.6,34.4375),(30.6,33.7),(30.6,33.6),(30.2,33.2),(30.2,33)]),
 ('UI_PLUS',31.85,33.25,[(31.4,34.4375),(31.4,33.9),(31.7,33.6),(31.85,33.45),(31.85,33.25)]),
 ('SAA_DTACK_N',32.6,33.25,[(32.6,34.4375),(32.6,33.25)]),
 ('CC1_ADC',33.3,33,[(33,34.4375),(33,33.9),(33.15,33.75),(33.15,33.15),(33.3,33)]),
 ('CC2_ADC',34,36.1,[(33.4,34.4375),(33.4,35.3),(34,35.9),(34,36.1)]),
 ('STATUS_LED',35,36.1,[(33.8,34.4375),(33.8,35.15),(34.75,36.1),(35,36.1)]),
 ('SOUND_ENABLE',35.3,35.4,[(34.2,34.4375),(34.2,34.975),(34.625,35.4),(35.3,35.4)]),
 ('MIDI_RX',27.85,26.1,[(28.5625,28.8),(27.85,28.8),(27.85,26.1)]),
 ('SAA_A0',28.65,25.85,[(28.5625,28.4),(28.75,28.4),(28.75,26.05),(28.65,25.95),(28.65,25.85)]),
 ('SAA_CLK_RAW',27.2,29,[(28.5625,29.2),(27.7,29.2),(27.2,29)]),
 ('GATE1_N',27.73,29.73,[(28.5625,29.6),(27.86,29.6),(27.73,29.73)]),
]:
 route(net,points,.20 if net in ['CC2_ADC','SOUND_ENABLE','STATUS_LED','UI_SCL','SAA_A0','SAA_CLK_RAW','MIDI_RX'] else .25 if net=='GATE1_N' else .15);via(net,(x,y),.5,.2)

# Remaining left-side RP2040 pins: separate front-layer escapes and staggered vias.
for net,points,end in [
 ('SWCLK',[(28.5625,32),(27.95,32),(27.7,31.75)],(27.7,31.75)),
 ('SWDIO',[(28.5625,32.4),(27.7,32.4),(27.55,32.55)],(27.55,32.55)),
 ('RUN',[(28.5625,32.8),(28.05,32.8),(27.2,33.65),(26.6,33.65)],(26.6,33.65)),
 ('SN_CLK_RAW',[(28.5625,33.2),(28.1,33.2),(27.55,33.75),(27.55,33.85),(27.8,34.1)],(27.8,34.1)),
 ('GATE2_N',[(28.5625,33.6),(28.8,33.6),(28.8,34.3)],(28.8,34.3)),
]:
 route(net,points,.15)
 via(net,end,.5,.2)

# Reserve control exits before the data routes occupy the MCU area.
route('SN2_CS_N',[(29.5,29.2),(28.9,29.8),(28.9,31.9)],.15,p.B_Cu)
route('SAA_CS_N',[(30.2,28.65),(30.2,29.35),(30.8,29.95),(30.8,31.15)],.15,p.B_Cu)
route('SN1_CS_N',[(29.8,26.5),(29.8,23.9)],.15,p.B_Cu)
route('WR_N',[(30.6,26.5),(30.6,24.25)],.15,p.B_Cu)
route('SAA_A0',[(28.65,25.85),(28,25.2),(27,25.2)],.15,p.B_Cu)

# Exclude through-via drills from every SMT copper land. Tracks and pours permitted.
for f in b.GetFootprints():
 for pad in f.Pads():
  if pad.GetAttribute()!=p.PAD_ATTRIB_SMD:continue
  # Rotated rectangle is conservative for roundrect SMD lands.
  import math
  cx=p.ToMM(pad.GetPosition().x)-100;cy=p.ToMM(pad.GetPosition().y)-100
  w=p.ToMM(pad.GetSize().x)/2;h=p.ToMM(pad.GetSize().y)/2;a=math.radians(pad.GetOrientationDegrees())
  xy=[(cx+dx*math.cos(a)+dy*math.sin(a),cy-dx*math.sin(a)+dy*math.cos(a)) for dx,dy in [(-w,-h),(w,-h),(w,h),(-w,h)]]
  for layer in [p.F_Cu,p.B_Cu]:keepout(xy,layer)
out=ROOT/'routing';out.mkdir(exist_ok=True)
p.SaveBoard(str(out/'priority.kicad_pcb'),b)
for ext in ['.kicad_pro','.kicad_dru']:shutil.copy2(ROOT/('hardware/Chiptune_USB_CV'+ext),out/('priority'+ext))
shutil.copytree(ROOT/'hardware/Chiptune.pretty',out/'Chiptune.pretty',dirs_exist_ok=True)
shutil.copy2(ROOT/'hardware/fp-lib-table',out/'fp-lib-table')
print('Saved priority nets:',len(b.GetTracks()),'tracks/vias')
