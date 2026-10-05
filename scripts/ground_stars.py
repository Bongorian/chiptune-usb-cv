#!/usr/bin/env python3
"""Explicitly connect fragile ground pads to plated contacts on the inner plane."""
import os,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
exec(compile(Path(__file__).with_name('grid_route.py').read_text().split('nets=sorted(')[0],'grid_route.py','exec'))
requested=['Y1.4','C19.2','C13.2','R6.2']
pad_ids={f.GetReference()+'.'+pd.GetNumber():pd.m_Uuid.AsString() for f in b.GetFootprints() for pd in f.Pads()}
reference=[r for r in items if r['net']=='GND' and (r['kind']=='via' or (r['kind']=='zone' and 2 in r['gs']))]
plane=next(r for r in reference if r['kind']=='zone' and r['gs'][2].area>1000)
reference=[r for r in reference if r['kind']=='zone' and r is plane or r['kind']=='via' and r['gs'][2].intersects(plane['gs'][2])]
for name in requested:
 target=[r for r in items if r['uid']==pad_ids[name]]
 result,info=masks('GND',reference,target)
 if result is None:
  result,info=masks('GND',reference,target,soft=True)
  if result is None:raise RuntimeError((name,info))
  bad=blockers('GND',result);excluded.update(bad);others={r['net'] for r in items if r['uid'] in bad}
  for other in others:prune(other)
  print('Ground star',name,'displaced',sorted(others),info,flush=True)
 else:print('Ground star',name,info,flush=True)
 emit('GND',result);save()
