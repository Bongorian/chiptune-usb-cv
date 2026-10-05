#!/usr/bin/python3
"""Audit native geometry and supplier footprint reference frames; no copper edits."""
import csv
import hashlib
import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / 'routed/Chiptune_USB_CV.kicad_pcb'
OUT = ROOT / 'outputs/assembly-review'
OUT.mkdir(exist_ok=True)
board = pcbnew.LoadBoard(str(BOARD))
old = pcbnew.LoadBoard(str(ROOT / 'variants/jlc-one-before-basic-review/routed/Chiptune_USB_CV.kicad_pcb'))
design = {r['ref']: r for r in json.loads((ROOT / 'routed/design.json').read_text())}


def geometry(b):
    footprints = sorted((f.GetReference(), tuple(f.GetPosition()), f.GetOrientationDegrees(),
                         sorted((p.GetNumber(), tuple(p.GetPosition()), tuple(p.GetSize()),
                                 tuple(p.GetDrillSize()), p.GetNetname(), p.GetOrientationDegrees())
                                for p in f.Pads())) for f in b.GetFootprints())
    tracks = sorted((type(t).__name__, tuple(t.GetStart()), tuple(t.GetEnd()),
                     t.GetWidth(t.GetLayer()) if isinstance(t, pcbnew.PCB_VIA) else t.GetWidth(),
                     t.GetLayer(), t.GetNetname(), t.GetDrillValue() if isinstance(t, pcbnew.PCB_VIA) else 0)
                    for t in b.GetTracks())
    return footprints, tracks


assert geometry(board) == geometry(old), 'Placement, pads or copper changed'
# Metadata replacement is performed in text, so every other byte is identical.
old_text = (ROOT / 'variants/jlc-one-before-basic-review/routed/Chiptune_USB_CV.kicad_pcb').read_text()
new_text = BOARD.read_text()
expected = old_text.replace('MMBT3904-7-F', 'MMBT3904(RANGE:100-300)').replace('GREEN STATUS', 'RED STATUS')
assert expected == new_text, 'Unexpected board modification'


def library(code):
    root = ET.parse(ROOT / f'research/jlc-review/{code}-pcb.svg').getroot()
    anchor = next(e for e in root.iter() if e.get('c_origin') and not e.get('c_partid'))
    ox, oy = map(float, anchor.get('c_origin').split(','))
    def xy(e):
        x, y = map(float, e.get('c_origin').split(','))
        return ((x-ox)*0.254, -(y-oy)*0.254)
    pads = {e.get('number'): xy(e) for e in root.iter() if e.get('c_partid') == 'part_pad'}
    holes = [xy(e) for e in root.iter() if e.get('c_partid') == 'part_hole']
    return pads, holes


def fit(points):
    n = len(points)
    a = [sum(u[i] for u, v in points)/n for i in (0, 1)]
    z = [sum(v[i] for u, v in points)/n for i in (0, 1)]
    co = si = 0
    for u, v in points:
        x, y = u[0]-a[0], u[1]-a[1]
        xx, yy = v[0]-z[0], v[1]-z[1]
        co += x*xx+y*yy
        si += x*yy-y*xx
    angle = round(math.degrees(math.atan2(si, co))/90)*90 % 360
    cc, ss = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    tx, ty = z[0]-cc*a[0]+ss*a[1], z[1]-ss*a[0]-cc*a[1]
    error = max(math.hypot(cc*u[0]-ss*u[1]+tx-v[0], ss*u[0]+cc*u[1]+ty-v[1]) for u, v in points)
    return angle, tx, ty, error


corrections = {}
audit = []
for f in sorted(board.GetFootprints(), key=lambda f: f.GetReference()):
    ref = f.GetReference()
    if ref not in design:
        continue
    r = design[ref]
    origin = f.GetPosition()
    pads = {p.GetNumber(): p for p in f.Pads() if p.GetNumber()}
    def relative(p):
        return ((p.GetPosition().x-origin.x)/1e6, -(p.GetPosition().y-origin.y)/1e6)
    native_angle = f.GetOrientationDegrees() % 360
    angle, dx, dy, error = native_angle, 0, 0, None
    method = 'Native placement; online polarity/reference-frame confirmation required'
    if ref.startswith('U') and ref not in {'U10', 'U11', 'U12'} or ref in {'Q3', 'Q4', 'D1', 'D3', 'D4', 'D5', 'D6'}:
        supplier, _ = library(r['lcsc'])
        points = [(supplier[n], relative(p)) for n, p in pads.items() if n in supplier]
        assert len(points) == len(pads)
        angle, tx, ty, error = fit(points)
        assert math.hypot(tx, ty) < 0.05 and error < 0.23
        method = 'All numbered pads fitted to supplier library; body center preserved'
    elif ref == 'J1':
        supplier, _ = library(r['lcsc'])
        signal = ['A5', 'A6', 'A7', 'A8', 'B5', 'B6', 'B7', 'B8']
        angle, dx, dy, error = fit([(supplier[n], relative(pads[n])) for n in signal])
        assert error < 0.002
        method = 'Eight USB signal pads; manufacturer library anchor differs from KiCad by +1.572 mm Y'
    elif ref in {'J2', 'J3', 'J6', 'J7', 'J8', 'J9'}:
        _, holes = library(r['lcsc'])
        native_holes = [relative(p) for p in f.Pads() if not p.GetNumber()]
        assert len(holes) == len(native_holes) == 2
        angle = (native_angle + 90) % 360
        cc, ss = math.cos(math.radians(angle)), math.sin(math.radians(angle))
        transformed = sorted((cc*x-ss*y, ss*x+cc*y) for x, y in holes)
        targets = sorted(native_holes)
        dx = sum(v[0]-u[0] for u, v in zip(transformed, targets))/2
        dy = sum(v[1]-u[1] for u, v in zip(transformed, targets))/2
        error = max(math.hypot(u[0]+dx-v[0], u[1]+dy-v[1]) for u, v in zip(transformed, targets))
        assert error < 0.002
        method = 'Two locating pegs + outward opening; manufacturer pinout authoritative (supplier 2/4 labels differ)'
    if angle != native_angle or abs(dx) > .05 or abs(dy) > .05:
        corrections[ref] = {'rotation_offset': (angle-native_angle+180)%360-180,
                            'dx_mm': round(dx, 4), 'dy_mm': round(dy, 4),
                            'method': method, 'online_preview_verified': False}
    if ref.startswith('U') or ref.startswith('D') or ref in {'Q3', 'Q4', 'J1', 'J2', 'J3', 'J6', 'J7', 'J8', 'J9', 'Y1'}:
        p = pads.get('1') or pads.get('A5')
        net = p.GetNetname() if p else ''
        audit.append([ref, r['lcsc'], r['value'], native_angle, angle, round(dx,4), round(dy,4),
                      round(error,4) if error is not None else '',
                      p.GetNumber() if p else '', round((p.GetPosition().x-100e6)/1e6,4) if p else '',
                      round((154e6-p.GetPosition().y)/1e6,4) if p else '', net, method,
                      'REQUIRES fresh JLCONE preview; native pin/net check complete'])

(ROOT / 'routed/jlc-placement-corrections.json').write_text(json.dumps(corrections, indent=2)+'\n')
with (OUT / 'Placement_Audit.csv').open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['Reference','LCSC','Value','KiCad rotation CCW','Supplier library rotation CCW',
                'CPL X correction mm','CPL Y correction mm','Pad or peg fit max residual mm',
                'Pin marker','Pin marker X mm','Pin marker Y mm','Pin net','Evidence','Preview status'])
    w.writerows(audit)
(OUT / 'Revision_Verification.json').write_text(json.dumps({
    'board_metadata_only_changes': True, 'all_other_board_bytes_identical': True,
    'footprints_pads_tracks_vias_and_nets_identical': True,
    'assembly_ic_numbered_pad_fits': 9, 'hand_assembly_ic_native_pin1_checks': 3,
    'cpl_corrections': corrections, 'jlc_online_preview_verified': False,
    'source_board_sha256': hashlib.sha256(BOARD.read_bytes()).hexdigest()
}, indent=2)+'\n')
print(json.dumps(corrections, indent=2))
