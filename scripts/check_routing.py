#!/usr/bin/python3
import json,collections
import pcbnew as p
from routing_rules import ROOT
q=ROOT/'routed/Chiptune_USB_CV.kicad_pcb';b=p.LoadBoard(str(q));source=p.LoadBoard(str(ROOT/'hardware/Chiptune_USB_CV.kicad_pcb'))
src={f.GetReference():f for f in source.GetFootprints()};errs=[]
if {f.GetReference() for f in b.GetFootprints()}!=set(src):errs.append('Footprint inventory changed')
if b.GetCopperLayerCount()!=4:errs.append('Expected four copper layers')
for f in b.GetFootprints():
 a=src[f.GetReference()]
 if a.GetPosition()!=f.GetPosition() or abs(a.GetOrientationDegrees()-f.GetOrientationDegrees())>.01:errs.append('Placement changed: '+f.GetReference())
 pn={x.GetNumber():x.GetNetname() for x in a.Pads() if x.GetNumber()}
 for pad in f.Pads():
  if pad.GetNumber() and pn.get(pad.GetNumber())!=pad.GetNetname():errs.append('Pad net changed: '+f.GetReference()+'-'+pad.GetNumber())
drc=json.loads((ROOT/'outputs/drc-routed.json').read_text());erc=json.loads((ROOT/'outputs/erc-routing.json').read_text())
erc_violations=[v for sheet in erc.get('sheets',[]) for v in sheet.get('violations',[])]
if drc['violations']:errs.append('DRC violations: '+str(len(drc['violations'])))
if drc['unconnected_items']:errs.append('Unconnected: '+str(len(drc['unconnected_items'])))
if erc_violations:errs.append('ERC violations: '+str(len(erc_violations)))
if drc.get('schematic_parity',[]):errs.append('Schematic parity: '+str(len(drc['schematic_parity'])))
vias=[t for t in b.GetTracks() if isinstance(t,p.PCB_VIA)];widths=collections.Counter(round(p.ToMM(t.GetWidth()),4) for t in b.GetTracks() if not isinstance(t,p.PCB_VIA))
r={'copper_layers':b.GetCopperLayerCount(),'placement_preserved':not any('Placement' in e for e in errs),'pad_nets_preserved':not any('Pad net' in e for e in errs),'footprints':len(src),'track_width_segments':dict(widths),'vias':len(vias),'drc_violations':len(drc['violations']),'unconnected':len(drc['unconnected_items']),'erc_violations':len(erc_violations),'schematic_parity_issues':len(drc.get('schematic_parity',[])),'issues':errs,'complete':not errs}
(ROOT/'outputs/routing-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));assert not errs
