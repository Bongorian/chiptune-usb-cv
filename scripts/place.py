#!/usr/bin/python3
"""Pack small passives around their electrical blocks, respecting physical courtyards."""
import pcbnew as p, json, math, sys
from pathlib import Path
boardpath='hardware/Chiptune_USB_CV.kicad_pcb'
b=p.LoadBoard(boardpath)
def v(x,y): return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
def bbox(fp):
 shapes=[s for s in fp.GraphicalItems() if s.GetLayer()==p.F_CrtYd]
 boxes=[s.GetBoundingBox() for s in shapes]
 if not boxes:boxes=[fp.GetBoundingBox(False,False)]
 return (min(p.ToMM(z.GetX()) for z in boxes),min(p.ToMM(z.GetY()) for z in boxes),max(p.ToMM(z.GetRight()) for z in boxes),max(p.ToMM(z.GetBottom()) for z in boxes))
def overlaps(a,bb,gap=.08):return a[0]<bb[2]+gap and a[2]>bb[0]-gap and a[1]<bb[3]+gap and a[3]>bb[1]-gap
fps={f.GetReference():f for f in b.GetFootprints()}
targets=json.loads(Path('scripts/layout_targets.json').read_text())
for r in ['JP1','JP2']:
 if fps[r].GetLayer()!=p.B_Cu:fps[r].Flip(fps[r].GetPosition(),False)
for ref,xyz in targets.items():
 fps[ref].SetPosition(v(100+xyz[0],100+xyz[1]))
 if len(xyz)==3:fps[ref].SetOrientationDegrees(xyz[2])
from flow_pack import pack
# Review coordinates are authoritative. Rebuilding must never repack the board.
optimize='--optimize' in sys.argv
optimized=pack(b,fps,targets,bbox,overlaps,v,p,freeze=not optimize)
if optimize:Path('outputs/flow-optimized-passives.json').write_text(json.dumps(optimized,indent=2)+'\n')
# References on Fab for dense passives; silk for socket IDs and connectors.
for f in b.GetFootprints():
 f.Reference().SetLayer(p.F_Fab if f.GetLayer()==p.F_Cu else p.B_Fab)
 f.Value().SetVisible(False)
 for s in list(f.GraphicalItems()):
  if s.GetLayer() in [p.F_SilkS,p.B_SilkS]:
   # Retain footprint outline data on Fab; rebuild readable silk after routing.
   s.SetLayer(p.F_Fab if s.GetLayer()==p.F_SilkS else p.B_Fab)
for s in list(b.GetDrawings()):
 if s.GetLayer() in [p.F_SilkS,p.Edge_Cuts]: s.SetLayer(p.Dwgs_User)
for a,z in [((103,100),(182,100)),((185,103),(185,151)),((182,154),(103,154)),((100,151),(100,103))]:
 e=p.PCB_SHAPE();e.SetShape(p.SHAPE_T_SEGMENT);e.SetLayer(p.Edge_Cuts);e.SetStart(v(*a));e.SetEnd(v(*z));e.SetWidth(p.FromMM(.05));b.Add(e)
for a,m,z in [((182,100),(184.12132,100.87868),(185,103)),((185,151),(184.12132,153.12132),(182,154)),((103,154),(100.87868,153.12132),(100,151)),((100,103),(100.87868,100.87868),(103,100))]:
 e=p.PCB_SHAPE();e.SetShape(p.SHAPE_T_ARC);e.SetLayer(p.Edge_Cuts);e.SetArcGeometry(v(*a),v(*m),v(*z));e.SetWidth(p.FromMM(.05));b.Add(e)
placements={f.GetReference():{'x':round(p.ToMM(f.GetPosition().x)-100,4),'y':round(p.ToMM(f.GetPosition().y)-100,4),'rot':f.GetOrientationDegrees(),'side':'B' if f.GetLayer()==p.B_Cu else 'F'} for f in b.GetFootprints()}
Path('outputs/placements.json').write_text(json.dumps(placements,indent=2))
p.SaveBoard(boardpath,b)
print('Placed',len(placements),'components')
