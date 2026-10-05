#!/usr/bin/python3
"""Pad-level rectilinear placement metric. Not routed lengths or rigorous bounds."""
import pcbnew as p,json,sys,math
from pathlib import Path

def evaluate(path):
 b=p.LoadBoard(str(path));fs={f.GetReference():f for f in b.GetFootprints()}
 ns={}
 for r,f in fs.items():
  for d in f.Pads():
   if d.GetNetname():ns.setdefault(d.GetNetname(),[]).append((r,d.GetNumber(),p.ToMM(d.GetPosition().x),p.ToMM(d.GetPosition().y)))
 def pin(ref,n):return next(v for v in ns.values() for v in v if v[0]==ref and v[1]==str(n))
 def dist(a,z):return abs(a[2]-z[2])+abs(a[3]-z[3])
 def mst(net):
  v=ns.get(net,[])
  if not v:return 0
  todo=set(range(1,len(v)));done=[0];total=0
  while todo:
   length,j=min((dist(v[i],v[j]),j) for i in done for j in todo);total+=length;done.append(j);todo.remove(j)
  return total
 def nets(names):return round(sum(mst(n) for n in names),2)
 groups={'midi_input':['MIDI_RING','MIDI_ANODE','MIDI_TIP'],'midi_to_mcu':['MIDI_RX'], 'cv_high_impedance':['CV1_NODE','CV2_NODE','CV_REF_NODE','CV_BIAS_DIV'], 'cv_buffered_to_adc':['CV1_BUF','CV2_BUF','CV_REF'], 'mixer_feedback':['SUM_L','SUM_R','VREF_DIV','UNUSED_OP_OUT'], 'mixer_input_ac':['SAA_L_AC','SAA_R_AC','SN1_OUT_AC','SN2_OUT_AC'], 'line_output':['MIX_L','MIX_R','LINE_L_AC','LINE_R_AC','LINE_L','LINE_R'],'usb':['USB_DP','USB_DM','USB_DP_MCU','USB_DM_MCU'],'flash':['FLASH_CS','FLASH_D0','FLASH_D1','FLASH_D2','FLASH_D3','FLASH_CLK'],'clocks':['XIN','XOUT','XTAL_OUT','SAA_CLK_RAW','SAA_CLK','SN_CLK_RAW','SN_CLK'],'sound_data_bus':[f'D{i}' for i in range(8)],'boost_switch':['BOOST_SW'],'boost_feedback':['BOOST_FB'],'gate_inputs':['GATE1_IN','GATE1_BASE','GATE2_IN','GATE2_BASE'],'gate_to_mcu':['GATE1_N','GATE2_N']}
 return {'file':str(path),'method':'Sum of per-net Manhattan minimum spanning trees over pad centres; excludes power/GND; geometric placement proxy only, not routed length or a rigorous bound.','blocks_mm':{g:nets(n) for g,n in groups.items()},'net_mm':{n:round(mst(n),2) for names in groups.values() for n in names},'boost_sw_pair_mm':round(dist(pin('U3',5),pin('L1',2)),3)}
if __name__=='__main__':
 before=evaluate(Path('variants/usb-cv-before-flow-review/hardware/Chiptune_USB_CV.kicad_pcb'));after=evaluate(Path('hardware/Chiptune_USB_CV.kicad_pcb'))
 out={'before':before,'after':after,'change_percent':{g:round((after['blocks_mm'][g]/v-1)*100,1) if v else 0 for g,v in before['blocks_mm'].items()}}
 Path('outputs/review/flow-metrics.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,ensure_ascii=False))
