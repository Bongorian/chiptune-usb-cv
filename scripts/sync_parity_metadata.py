#!/usr/bin/python3
"""Synchronize named NC pads and mechanical-only footprint metadata."""
import xml.etree.ElementTree as E
import pcbnew as p
from routing_rules import apply_to_board,ROOT
root=E.parse(ROOT/'outputs/current-netlist.xml');nc=[]
for net in root.findall('.//nets/net'):
 if net.attrib['name'].startswith('unconnected-'):
  for node in net.findall('node'):nc.append((node.attrib['ref'],node.attrib['pin'],net.attrib['name']))
for path in [ROOT/'hardware/Chiptune_USB_CV.kicad_pcb',ROOT/'routed/Chiptune_USB_CV.kicad_pcb']:
 b=p.LoadBoard(str(path));fp={f.GetReference():f for f in b.GetFootprints()}
 for ref,pin,name in nc:
  if name not in b.GetNetsByName():b.Add(p.NETINFO_ITEM(b,name))
  for pad in fp[ref].Pads():
   if pad.GetNumber()==pin:
    if pad.GetNetname() and not pad.GetNetname().startswith('unconnected-'):raise ValueError((ref,pin,pad.GetNetname()))
    pad.SetNet(b.GetNetsByName()[name])
 for ref in ['H1','H2','H3','H4']:fp[ref].SetAttributes(fp[ref].GetAttributes() | p.FP_BOARD_ONLY | p.FP_EXCLUDE_FROM_BOM | p.FP_EXCLUDE_FROM_POS_FILES)
 apply_to_board(b);p.SaveBoard(str(path),b)
 print(path.name,'named NC pads',len(nc))
