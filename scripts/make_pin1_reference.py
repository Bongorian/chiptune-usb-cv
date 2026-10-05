#!/usr/bin/env python3
"""Annotate the exported native Fab drawing, without altering PCB geometry."""
import csv
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'outputs/jlc-one/reference/Assembly_Top.svg').read_text()
# Plot-origin offset confirmed by a known round DIP pad, U10 pin 1.
assert 'cx="8.0750" cy="12.3250"' in source
root = ET.fromstring(source)
inner = ''.join(ET.tostring(e, encoding='unicode') for e in root if e.tag.endswith('g'))
marks = []
for row in csv.DictReader((ROOT / 'outputs/assembly-review/Placement_Audit.csv').open()):
    ref = row['Reference']
    if not ref.startswith(('U', 'D', 'Q')) or row['Pin marker'] != '1':
        continue
    x, y = float(row['Pin marker X mm']), 54-float(row['Pin marker Y mm'])
    label = 'K' if ref in {'D1', 'D2', 'D5', 'D6'} else '1'
    marks.append(f'<circle cx="{x}" cy="{y}" r=".48" fill="#ed325f" stroke="white" stroke-width=".10"/>'
                 f'<text x="{x}" y="{y+.24}" text-anchor="middle" font-family="sans-serif" '
                 f'font-weight="bold" font-size=".72" fill="white">{label}</text>')
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1456" height="1056" viewBox="-3 -7 91 66">
<rect x="-3" y="-7" width="91" height="66" fill="#fff"/>
<text x="0" y="-4.5" font-family="sans-serif" font-size="1.7" font-weight="bold" fill="#163c40">CHIPTUNE - PLACEMENT / PIN 1 REFERENCE</text>
<text x="0" y="-2.3" font-family="sans-serif" font-size="1.05" fill="#516168">Top view | 85 x 54 mm | Pink: IC pin 1 / diode cathode K | Native PCB geometry</text>
<g transform="translate(-.575,-.025)">{inner}</g>{''.join(marks)}
<text x="0" y="57" font-family="sans-serif" font-size="1.02" fill="#ac294a">Compare every pin marker and connector opening with a FRESH JLCONE preview. Preview confirmation pending.</text></svg>'''
(ROOT / 'outputs/assembly-review/Placement_Pin1.svg').write_text(svg)
