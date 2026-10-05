#!/usr/bin/env python3
"""Iteratively remove native dead ends, checking connectivity after each edit."""
import subprocess,shutil,json,collections,sys
from pathlib import Path
prev=sys.argv[1];stem=sys.argv[2]
for i in range(1,61):
 out=f'{stem}-{i}';subprocess.run(['.venv/bin/python','scripts/cleanup_route_ends.py',prev+'.kicad_pcb',prev+'-drc.json',out+'.kicad_pcb'],check=True,stdout=subprocess.DEVNULL)
 for ext in ['kicad_pro','kicad_dru']:shutil.copyfile('hardware/Chiptune_USB_CV.'+ext,out+'.'+ext)
 subprocess.run(['kicad-cli','pcb','drc','--format','json','--severity-all','-o',out+'-drc.json',out+'.kicad_pcb'],check=True,stdout=subprocess.DEVNULL)
 d=json.load(open(out+'-drc.json'));print(i,collections.Counter(v['type'] for v in d['violations']),'unconnected',len(d['unconnected_items']),flush=True)
 if d['unconnected_items']:raise RuntimeError('Cleanup changed connectivity; retain previous board '+prev)
 if not d['violations']:Path(stem+'-result.txt').write_text(out+'\n');break
 prev=out
else:raise RuntimeError('Cleanup did not converge')
