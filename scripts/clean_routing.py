#!/usr/bin/python3
"""Remove duplicate SES copper; operate on native blocks without changing pads."""
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'routed/Chiptune_USB_CV.kicad_pcb').read_text();seen=set();counts={'segment':0,'via':0,'zone':0}
def nums(block,tag):
 m=re.search(r'\('+tag+r' ([^)]+)\)',block)
 return tuple(round(float(x),4) for x in m[1].split()) if m else ()
def clean(m):
 block=m[0];kind=m[1]
 if kind=='zone':
  if '(keepout' not in block:counts[kind]+=1;return ''
  return block
 net=nums(block,'net')
 if kind=='via':
  size=nums(block,'size')[0];drill={.5:.2,.6:.3,.8:.4}.get(size,.3)
  block=re.sub(r'\(drill [^)]+\)',f'(drill {drill})',block)
  key=(kind,net,nums(block,'at'),size)
 else:
  a=nums(block,'start');c=nums(block,'end');width=nums(block,'width')
  layer=re.search(r'\(layer "([^"]+)"\)',block)[1]
  key=(kind,net,tuple(sorted([a,c])),width,layer)
 if key in seen:counts[kind]+=1;return ''
 seen.add(key);return block
s=re.sub(r'^\t\((segment|via|zone)\n.*?^\t\)\n',clean,s,flags=re.M|re.S)
(ROOT/'routing/refine.kicad_pcb').write_text(s)
print('Removed duplicates/pours',counts)
