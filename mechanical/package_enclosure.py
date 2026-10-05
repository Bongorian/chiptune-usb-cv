#!/usr/bin/env python3
"""Validate and package the enclosure print files and editable CAD sources."""
import ast
import hashlib
import json
import re
import zipfile
from pathlib import Path
import trimesh

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
export=HERE/'exports'
report=json.loads((HERE/'mechanical_verification.json').read_text())
features=json.loads((HERE/'board_features.json').read_text())
assert report['pass']
assert hashlib.sha256((ROOT/'routed/Chiptune_USB_CV.kicad_pcb').read_bytes()).hexdigest()==features['pcb_sha256']
for path in HERE.glob('*.py'):ast.parse(path.read_text())
for link in re.findall(r'\]\(([^)]+)\)',(HERE/'README.md').read_text()):
 if '://' not in link:assert (HERE/link).exists(),link
for name in ['base','lid','button_cap','fit_coupon']:
 mesh=trimesh.load(export/(name+'.stl'),force='mesh')
 assert mesh.is_watertight and mesh.is_winding_consistent and mesh.volume>0
 assert len(mesh.split(only_watertight=False))==1
 assert abs(mesh.bounds[0,2])<1e-4
assert report['additional_transparent_parts']==0
assert not list(export.glob('*_window_cut.dxf'))
report['inputs_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [HERE/'parameters.json',HERE/'board_features.json',HERE/'build_enclosure.py']}
(HERE/'mechanical_verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
files=[p for p in HERE.iterdir() if p.is_file() and p.suffix in ['.py','.json','.md','.txt']]
files += [p for p in export.iterdir() if p.suffix in ['.stl','.step','.blend']]
files += [p for p in (HERE/'previews').iterdir() if p.name in ['assembled.png','top.png','exploded.png','base_inside.png','mechanical_drawing.svg','mechanical_drawing.png','mechanical_drawing.pdf']]
files=sorted(files)
(export/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(HERE))+'\n' for p in files))
files.append(export/'SHA256SUMS.txt')
archive_path=export/'Chiptune_Enclosure_3DPrint.zip'
with zipfile.ZipFile(archive_path,'w',zipfile.ZIP_DEFLATED) as archive:
 for p in files:archive.write(p,'Chiptune_Enclosure/'+str(p.relative_to(HERE)))
with zipfile.ZipFile(archive_path) as archive:assert archive.testzip() is None
print('Verified and packaged',len(files),'files;',archive_path.stat().st_size,'bytes')
