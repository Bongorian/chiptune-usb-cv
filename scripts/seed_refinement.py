#!/usr/bin/python3
import pcbnew as p,shutil
from routing_rules import ROOT,apply_to_board,classify
b=p.LoadBoard(str(ROOT/'routing/priority.kicad_pcb'));old=p.LoadBoard(str(ROOT/'routing/grid-v8.kicad_pcb'));previous=p.LoadBoard(str(ROOT/'routing/priority-pre-controls.kicad_pcb'));manualids={t.m_Uuid.AsString() for t in previous.GetTracks()};ns=b.GetNetsByName();newsourceids={t.m_Uuid.AsString() for t in b.GetTracks()};n=0
for t in old.GetTracks():
 net=t.GetNetname()
 if t.m_Uuid.AsString() in manualids:continue
 c=t.Duplicate();c.SetNet(ns[net]);b.Add(c);n+=1
apply_to_board(b);p.SaveBoard(str(ROOT/'routing/refined.kicad_pcb'),b)
for ext in ['pro','dru']:shutil.copy(ROOT/f'hardware/Chiptune_USB_CV.kicad_{ext}',ROOT/f'routing/refined.kicad_{ext}')
import json
(ROOT/'routing/seed-autoids.json').write_text(json.dumps([t.m_Uuid.AsString() for t in b.GetTracks() if t.m_Uuid.AsString() not in newsourceids]))
print('Seeded',n,'existing route objects')
