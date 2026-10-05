#!/usr/bin/python3
"""Mechanical evidence for USB/CV placement revision; no claim of routed validation."""
import json, math
from pathlib import Path
import pcbnew as p
b=p.LoadBoard('hardware/Chiptune_USB_CV.kicad_pcb');fs={f.GetReference():f for f in b.GetFootprints()}
def xy(v):return [round(p.ToMM(v.x)-100,4),round(p.ToMM(v.y)-100,4)]
def pin(ref,n):return next(xy(d.GetPosition()) for d in fs[ref].Pads() if d.GetNumber()==n)
def distance(a,c):return round(math.dist(a,c),3)
drc=json.loads(Path('outputs/drc-placement.json').read_text());erc=json.loads(Path('outputs/erc.json').read_text())
report={'board_mm':[85,54],'tracks_and_vias':len(list(b.GetTracks())), 'placement_drc_violations':len(drc['violations']),'unconnected_items':len(drc['unconnected_items']), 'schematic_erc_violations':sum(len(s['violations']) for s in erc['sheets']), 'corner_centers_mm':[], 'controls':{r:xy(fs[r].GetPosition()) for r in ['SW5','SW6','SW7']}, 'jack_centers':{r:xy(fs[r].GetPosition()) for r in ['J6','J7','J2','J8','J9','J3']},'sound_bypass':{},'other_bypass':{},'oled':{'body_mm':[27.3,27.8],'center_mm':xy(fs['J5'].GetPosition()),'spacer_mm':5,'back_component_max_height_mm':1,'screw_head_keepout_diameter_mm':4.1,'mount_holes_mm':[xy(d.GetPosition()) for d in fs['J5'].Pads() if d.GetAttribute()==p.PAD_ATTRIB_NPTH]},'gpio_free':[23,24],'usb_device_only':True,'battery_removed':True}
for r,c1,c2,n in [('U10','C28','C31','18'),('U11','C29','C32','16'),('U12','C30','C33','16')]:
 a=pin(r,n);report['sound_bypass'][r]={'vdd_pad':a,'100n_center_distance_mm':distance(a,xy(fs[c1].GetPosition())),'4.7u_center_distance_mm':distance(a,xy(fs[c2].GetPosition())),'orientation_deg':fs[c1].GetOrientationDegrees()}
for r,c,n in [('U15','C57','4'),('U16','C58','8'),('U14','C36','4'),('U13','C34','8')]:report['other_bypass'][r]={'vdd_pad':pin(r,n),'capacitor':c,'center_distance_mm':distance(pin(r,n),xy(fs[c].GetPosition()))}
for s in b.GetDrawings():
 if s.GetLayer()==p.Edge_Cuts and s.GetShape()==p.SHAPE_T_ARC:report['corner_centers_mm'].append({'center':xy(s.GetCenter()),'radius_mm':round(p.ToMM(s.GetRadius()),4)})
assert report['tracks_and_vias']==0
assert report['placement_drc_violations']==report['schematic_erc_violations']==0
assert {tuple(a['center']) for a in report['corner_centers_mm']}=={(3,3),(82,3),(82,51),(3,51)}
assert [xy(fs[r].GetPosition()) for r in ['SW5','SW6','SW7']]==[[48,50],[57,50],[66,50]]
assert 'BT1' not in fs
Path('outputs/review/geometry-check.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False))
