#!/usr/bin/python3
"""Extract enclosure datums from the routed KiCad PCB (coordinates in mm)."""
import hashlib
import json
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / 'routed/Chiptune_USB_CV.kicad_pcb'
b = pcbnew.LoadBoard(str(PCB))
x0, y0, width, depth = 100.0, 100.0, 85.0, 54.0

def point(p):
    return [round(pcbnew.ToMM(p.x)-x0, 5), round(depth-(pcbnew.ToMM(p.y)-y0), 5)]

features = {}
for f in b.GetFootprints():
    ref = f.GetReference()
    if ref.startswith(('H', 'J', 'SW')) or ref in ['U10', 'U11', 'U12', 'D2']:
        features[ref] = dict(value=f.GetValue(), center=point(f.GetPosition()),
                             rotation_deg=f.GetOrientationDegrees(), pads=[])
        for p in f.Pads():
            features[ref]['pads'].append(dict(number=p.GetNumber(), center=point(p.GetPosition()),
                drill_mm=[round(pcbnew.ToMM(p.GetDrillSize().x), 4),round(pcbnew.ToMM(p.GetDrillSize().y), 4)]))
    # Small-component XY envelopes, used only for conservative mechanical checks.
    if ref not in features and not ref.startswith(('JP','FB')):
        pts=[]
        for g in f.GraphicalItems():
            if isinstance(g, pcbnew.PCB_SHAPE) and g.GetLayerName()=='F.Fab':
                pts.extend([point(g.GetStart()),point(g.GetEnd())])
        if pts:
            features[ref]=dict(value=f.GetValue(), center=point(f.GetPosition()),
                fab_bounds=[min(p[0] for p in pts),min(p[1] for p in pts),max(p[0] for p in pts),max(p[1] for p in pts)])

out=dict(units='mm', pcb_sha256=hashlib.sha256(PCB.read_bytes()).hexdigest(),
    pcb_origin_kicad_mm=[x0,y0], coordinates='X right, Y toward sound-IC row, Z above PCB',
    width_mm=width, depth_mm=depth, thickness_mm=pcbnew.ToMM(b.GetDesignSettings().GetBoardThickness()),
    corner_radius_mm=3.0, features=features)
(ROOT/'mechanical/board_features.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print('Extracted', len(features), 'footprint datums; PCB thickness',out['thickness_mm'])
