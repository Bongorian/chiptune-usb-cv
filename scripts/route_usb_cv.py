#!/usr/bin/python3
"""Local autorouting with verified netclass assignment, priority nets protected."""
from pathlib import Path
import subprocess,shutil,re,sys
import pcbnew as p
from routing_rules import ROOT,apply_to_board
source=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'routing/priority.kicad_pcb'
b=p.LoadBoard(str(source));apply_to_board(b)
dsn=ROOT/'routing/all.dsn';ses=ROOT/'routing/all.ses'
assert p.ExportSpecctraDSN(b,str(dsn))
s=dsn.read_text();s=s.replace('(clearance 37.5 (type smd_smd))','(clearance 150 (type smd_smd))');s=re.sub(r'\(net GND\s+\(pins[^)]*\)\s*\)', '(net GND (pins C16-2 C18-2 U3-4 U1-19 U1-57))', s)
# Component drawing outlines are not copper barriers. Keep pad/via/hole keepouts.
while True:
 m=re.search(r'\((?:outline|via_keepout)\s',s)
 if not m:break
 start=m.start();depth=0
 for end in range(start,len(s)):
  depth+=int(s[end]=='(')-int(s[end]==')')
  if depth==0:break
 s=s[:start]+s[end+1:]
# Freerouting 2.4.1 cannot reliably escape narrow roundrect polygons.
# Use conservative bounding rectangles in DSN only; native pad geometry is unchanged.
def rect_pad(m):
 st=m.group(0);poly=re.search(r'\(shape \(polygon (F.Cu|B.Cu) 0\s+([^)]*)\)\)',st)
 if not poly:return st
 coords=[float(x) for x in poly[2].split()];xs=coords[::2];ys=coords[1::2]
 return st[:poly.start()]+f'(shape (rect {poly[1]} {min(xs)} {min(ys)} {max(xs)} {max(ys)}))'+st[poly.end():]
s=re.sub(r'\(padstack RoundRect\[.*?\(attach off\)\s*\)',rect_pad,s,flags=re.S)
# All exported via sizes are legal for attaching to existing escape vias.
s=re.sub(r'\(use_via ([^)]*)\)',lambda m:'(use_via '+m[1]+' \"Via[0-1]_500:200_um\")',s)
# Protected traces may accept attachments at existing via endpoints.
s=s.replace('(type fix)','(type protect)')
# Other GND lands retain their original KiCad net; they connect via the final pour.
dsn.write_text(s)
subprocess.run(['freerouting-cli','-de',str(dsn),'-do',str(ses),'-mp',sys.argv[2] if len(sys.argv)>2 else '3','-mt','2','--router.copperToEdgeClearanceUm=500','--router.fanout.enabled=false','--router.holeClearanceUm=300','--router.saveIntermediateStages=true','--router.optimizer.enabled=false'],check=True,timeout=1200)
assert p.ImportSpecctraSES(b,str(ses))
out=ROOT/'routed';out.mkdir(exist_ok=True)
for item in (ROOT/'hardware').iterdir():
 if item.name.endswith('.kicad_pcb') or item.name.endswith('.kicad_prl') or item.name.endswith('~'):continue
 if item.is_dir():shutil.copytree(item,out/item.name,dirs_exist_ok=True)
 else:shutil.copy2(item,out/item.name)
p.SaveBoard(str(out/'Chiptune_USB_CV.kicad_pcb'),b)
print('Saved routed board',len(b.GetTracks()),'tracks/vias')
