#!/usr/bin/python3
import pcbnew as p
import design
b=p.LoadBoard(str(design.HW/(design.NAME+'.kicad_pcb')))
for f in b.GetFootprints():
 ref=f.GetReference();name='Board_'+ref
 f.SetFPID(p.LIB_ID('Chiptune',name))
 g=f.Duplicate()
 if g.GetLayer()==p.B_Cu:g.Flip(g.GetPosition(),False)
 g.SetPosition(p.VECTOR2I(0,0));g.SetOrientationDegrees(0)
 p.FootprintSave(str(design.LIB),g)
 for obj in design.parts:
  if obj['ref']==ref:
   obj.update(fp='Chiptune:'+name,x=p.ToMM(f.GetPosition().x)-100,y=p.ToMM(f.GetPosition().y)-100,rot=f.GetOrientationDegrees(),side='B' if f.GetLayer()==p.B_Cu else 'F')
p.SaveBoard(str(design.HW/(design.NAME+'.kicad_pcb')),b)
design.build_schematics();design.bom()
print('Saved project-specific footprint variants; no physical pad changes')
