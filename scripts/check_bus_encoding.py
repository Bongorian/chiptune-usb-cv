#!/usr/bin/python3
"""Check every bus byte against PCB pin wiring and IC bit significance."""
import subprocess,json,re
import pcbnew as p
from routing_rules import ROOT
q=ROOT/'routing/bus-codec-check.cpp'
q.write_text('#include "../firmware/sound_bus.h"\n#include <stdio.h>\nint main(){for(int i=0;i<256;i++)printf("%u %u\\n",chiptune_saa_gpio_word(i),chiptune_sn_gpio_word(i));}\n')
exe=ROOT/'routing/bus-codec-check'
subprocess.run(['g++','-std=c++17',str(q),'-o',str(exe)],check=True)
encoded=[tuple(map(int,line.split())) for line in subprocess.check_output([str(exe)],text=True).splitlines()]
b=p.LoadBoard(str(ROOT/'hardware/Chiptune_USB_CV.kicad_pcb'))
sn_pins=[3,2,1,15,13,12,11,10];checks=0
pins={int(a):int(c) for a,c in re.findall(r'#define PIN_D([0-7]) (\d+)',(ROOT/'firmware/pinmap.h').read_text())}
for ref in ['U10','U11','U12']:
 f=b.FindFootprintByReference(ref);pads={int(x.GetNumber()):x.GetNetname() for x in f.Pads()}
 for value in range(256):
  gpio=encoded[value][0 if ref=='U10' else 1];decoded=0
  for bit,pin in enumerate(range(10,18) if ref=='U10' else sn_pins):
   busbit=int(pads[pin][1:]);chipbit=bit if ref=='U10' else 7-bit
   decoded|=((gpio>>pins[busbit])&1)<<chipbit
  assert decoded==value,(ref,value,decoded)
  checks+=1
r={'complete':True,'bytes_per_chip':256,'chips':3,'checks':checks,'gpio_word_is_logical_byte':True,'gpio_for_board_D0_to_D7':[pins[i] for i in range(8)]}
(ROOT/'outputs/bus-encoding-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
