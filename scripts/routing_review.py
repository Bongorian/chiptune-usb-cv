#!/usr/bin/python3
"""Actual copper plots plus per-net measured widths and routed lengths."""
import json,csv,subprocess,collections,re
import pcbnew as p
from routing_rules import ROOT,classify,CLASSES
q=ROOT/'routed/Chiptune_USB_CV.kicad_pcb';b=p.LoadBoard(str(q));out=ROOT/'outputs/review';out.mkdir(exist_ok=True)
for name,layers,mirror in [('routing-front','F.Cu,F.SilkS,Edge.Cuts',False),('routing-inner1','In1.Cu,Edge.Cuts',False),('routing-ground','In2.Cu,Edge.Cuts',False),('routing-back','B.Cu,B.SilkS,Edge.Cuts',True),('routing-both','F.Cu,In1.Cu,B.Cu,F.SilkS,Edge.Cuts',False)]:
 subprocess.run(['kicad-cli','pcb','export','svg','--mode-single','--page-size-mode','2','--exclude-drawing-sheet','--layers',layers,'-o',str(out/(name+'.svg'))]+(['--mirror'] if mirror else [])+[str(q)],check=True)
display=out/'copper-display.kicad_pcb'
display.write_text(re.sub(r'^\t\(zone\n.*?^\t\)\n',lambda m:m[0] if '(keepout' in m[0] else '',q.read_text(),flags=re.M|re.S))
for name,layers,mirror in [('routing-traces-front','F.Cu,F.SilkS,Edge.Cuts',False),('routing-traces-back','B.Cu,B.SilkS,Edge.Cuts',True),('routing-traces-all','F.Cu,In1.Cu,B.Cu,F.SilkS,Edge.Cuts',False)]:
 subprocess.run(['kicad-cli','pcb','export','svg','--mode-single','--page-size-mode','2','--exclude-drawing-sheet','--layers',layers,'-o',str(out/(name+'.svg'))]+(['--mirror'] if mirror else [])+[str(display)],check=True)
stats={n.GetNetname():{'class':classify(n.GetNetname()),'widths_mm':set(),'length_mm':0.,'vias':0} for n in b.GetNetsByName().values() if n.GetNetname()}
for t in b.GetTracks():
 s=stats[t.GetNetname()]
 if isinstance(t,p.PCB_VIA):s['vias']+=1
 else:s['widths_mm'].add(round(p.ToMM(t.GetWidth()),4));s['length_mm']+=p.ToMM(t.GetLength())
for s in stats.values():s['widths_mm']=sorted(s['widths_mm']);s['length_mm']=round(s['length_mm'],3)
(out/'routing-stats.json').write_text(json.dumps(stats,indent=2)+'\n')
with (ROOT/'outputs/routing-widths.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['Net','Class','Preferred_width_mm','Copper_clearance_mm','Actual_widths_mm','Routed_length_mm','Via_count'])
 for net,s in sorted(stats.items()):w.writerow([net,s['class'],*CLASSES[s['class']][:2],';'.join(map(str,s['widths_mm'])),s['length_mm'],s['vias']])
print('Track segments',sum(not isinstance(t,p.PCB_VIA) for t in b.GetTracks()),'vias',sum(isinstance(t,p.PCB_VIA) for t in b.GetTracks()))
