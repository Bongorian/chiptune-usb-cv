#!/usr/bin/python3
"""JLCPCB 4-layer, outer 1 oz routing policies; applied to both project and DSN export."""
import json
from pathlib import Path
import pcbnew as p

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'hardware/Chiptune_USB_CV.kicad_pro'
# (preferred track width, copper clearance, via diameter, drill)
CLASSES={
 'Default':(.15,.15,.60,.30),
 'USB_Pair':(.25,.15,.60,.30),
 'Data_Control':(.15,.15,.50,.20),
 'QSPI':(.15,.15,.60,.30),
 'Clock':(.20,.15,.60,.30),
 'Clock_Source':(.15,.15,.60,.30),
 'Crystal':(.15,.20,.60,.30),
 'CV_Analog':(.25,.20,.60,.30),
 'Audio':(.25,.20,.60,.30),
 'Line_Output':(.30,.20,.60,.30),
 'MIDI_Isolated':(.25,.50,.60,.30),
 'Gate':(.20,.15,.60,.30),
 'Logic_Power':(.45,.15,.60,.30),
 'Core_Power':(.35,.15,.60,.30),
 'Sound_Power':(.75,.15,.80,.40),
 'Analog_Power':(.50,.15,.60,.30),
 'USB_Power':(1.00,.15,.80,.40),
 'Boost_Switch':(.75,.15,.60,.30),
 'Ground':(.50,.15,.60,.30),
}

def classify(net):
 if net=='GND':return 'Ground'
 if net=='USB_VBUS':return 'USB_Power'
 if net=='+5V':return 'Sound_Power'
 if net=='+5V_A':return 'Analog_Power'
 if net in ['+3V2','+3V2_A']:return 'Logic_Power'
 if net=='+1V1':return 'Core_Power'
 if net=='BOOST_SW':return 'Boost_Switch'
 if net in ['USB_D_P','USB_D_N','USB_MCU_D_P','USB_MCU_D_N']:return 'USB_Pair'
 if net.startswith('FLASH_'):return 'QSPI'
 if net in ['XIN','XOUT','XTAL_OUT']:return 'Crystal'
 if net in ['SAA_CLK_RAW','SN_CLK_RAW']:return 'Clock_Source'
 if net in ['SAA_CLK','SN_CLK']:return 'Clock'
 if net.startswith('CV'):return 'CV_Analog'
 if net.startswith('LINE_'):return 'Line_Output'
 if net in ['SAA_L','SAA_R','SAA_IREF','SAA_L_AC','SAA_R_AC','SN1_OUT','SN2_OUT','SN1_OUT_AC','SN2_OUT_AC','SUM_L','SUM_R','MIX_L','MIX_R','VREF','VREF_DIV','UNUSED_OP_OUT']:return 'Audio'
 if net in ['MIDI_TIP','MIDI_RING','MIDI_ANODE']:return 'MIDI_Isolated'
 if net.startswith('GATE'):return 'Gate'
 if net in [*[f'D{i}' for i in range(8)],'WR_N','SAA_CS_N','SN1_CS_N','SN2_CS_N','SAA_A0','SAA_DTACK_N']:return 'Data_Control'
 return 'Default'

def apply_to_board(b):
 ns=b.GetDesignSettings().m_NetSettings
 # LoadBoard alone does not load project netclass assignments.
 ns.ClearNetclasses();ns.ClearNetclassPatternAssignments()
 for name,(width,clearance,via,drill) in CLASSES.items():
  c=ns.GetDefaultNetclass() if name=='Default' else p.NETCLASS(name)
  c.SetTrackWidth(p.FromMM(width));c.SetClearance(p.FromMM(clearance))
  c.SetViaDiameter(p.FromMM(via));c.SetViaDrill(p.FromMM(drill))
  c.SetDiffPairWidth(p.FromMM(.25));c.SetDiffPairGap(p.FromMM(.15));c.SetDiffPairViaGap(p.FromMM(.25))
  if name!='Default':ns.SetNetclass(name,c)
 for net in b.GetNetsByName().values():
  if net.GetNetname():ns.SetNetclassPatternAssignment(net.GetNetname(),classify(net.GetNetname()))
 ns.RecomputeEffectiveNetclasses()
 for net in b.GetNetsByName().values():net.SetNetClass(ns.GetEffectiveNetClass(net.GetNetname()))
 ds=b.GetDesignSettings();ds.m_MinClearance=p.FromMM(.15);ds.m_TrackMinWidth=p.FromMM(.15)
 ds.m_CopperEdgeClearance=p.FromMM(.5);ds.m_ViasMinSize=p.FromMM(.5);ds.m_ViasMinAnnularWidth=p.FromMM(.15)
 ds.m_MinThroughDrill=p.FromMM(.20)
 ds.m_SilkClearance=p.FromMM(.15);ds.m_MinSilkTextHeight=p.FromMM(1.0);ds.m_MinSilkTextThickness=p.FromMM(.15)
 ds.m_HoleToHoleMin=p.FromMM(.30);ds.m_HoleClearance=p.FromMM(.30)
 ds.m_TentViasFront=True;ds.m_TentViasBack=True

def write_project():
 b=p.LoadBoard(str(PROJECT.with_suffix('.kicad_pcb')))
 data=json.loads(PROJECT.read_text());ds=data['board']['design_settings']
 ds['rules'].update(min_clearance=.15,min_track_width=.15,min_copper_edge_clearance=.50,
  min_via_diameter=.50,min_via_annular_width=.15,min_through_hole_diameter=.20,
  min_hole_clearance=.30,min_hole_to_hole=.30,min_silk_clearance=.15,min_text_height=1.0,min_text_thickness=.15)
 ds['track_widths']=[0,.15,.20,.25,.30,.35,.45,.50,.75,1.0]
 ds['via_dimensions']=[{'diameter':.5,'drill':.2},{'diameter':.6,'drill':.3},{'diameter':.8,'drill':.4}]
 ds['diff_pair_dimensions']=[{'width':.25,'gap':.15,'via_gap':.25}]
 template=data['net_settings']['classes'][0]
 classes=[]
 for i,(name,(width,clearance,via,drill)) in enumerate(CLASSES.items()):
  c=dict(template);c.update(name=name,track_width=width,clearance=clearance,via_diameter=via,via_drill=drill,
   diff_pair_width=.25,diff_pair_gap=.15,diff_pair_via_gap=.25,priority=2147483647 if name=='Default' else i)
  classes.append(c)
 data['net_settings'].update(classes=classes,netclass_assignments=None,
  netclass_patterns=[{'netclass':classify(n.GetNetname()),'pattern':n.GetNetname()} for n in b.GetNetsByName().values() if n.GetNetname()])
 PROJECT.write_text(json.dumps(data,indent=2)+'\n')
 # Preferred widths govern DSN routing. Neckdowns are allowed only at dense ICs.
 rules=['(version 1)','(rule "PTH ring" (condition "A.Pad_Type == \'Through-hole\'") (constraint annular_width (min 0.18mm)))',
 '(rule "Component holes" (condition "A.Type == \'Pad\' && B.Type == \'Pad\'") (constraint hole_to_hole (min 0.45mm)))',
 '(rule "No via drill in SMD lands" (condition "A.Type == \'Via\' && B.Pad_Type == \'SMD\'") (constraint physical_hole_clearance (min 0.15mm)))']
 for name,(width,clearance,_,_) in CLASSES.items():
  minimum=.25 if name=='Ground' else width
  rules.append(f'(rule "{name} width" (condition "A.hasNetclass(\'{name}\')") (constraint track_width (min {minimum}mm) (opt {width}mm)))')
 rules.append('(rule "Converter VIN bias branch" (condition "A.NetName == \'USB_VBUS\' && A.intersectsArea(\'BOOST_VIN_BRANCH\')") (constraint track_width (min 0.25mm)))')
 rules.append('(rule "Small power IC lead escapes" (condition "A.Type == \'Track\' && (A.hasNetclass(\'USB_Power\') || A.hasNetclass(\'Logic_Power\') || A.hasNetclass(\'Ground\')) && (A.intersectsCourtyard(\'U5\') || A.intersectsCourtyard(\'U9\'))") (constraint track_width (min 0.25mm)))')
 # A segment intersecting the courtyard may neck down to enter a small lead.
 rules.append('(rule "Fine-pitch IC escapes" (condition "A.Type == \'Track\' && (A.intersectsCourtyard(\'U1\') || A.intersectsCourtyard(\'U3\') || A.intersectsCourtyard(\'U14\') || A.intersectsCourtyard(\'U15\') || A.intersectsCourtyard(\'U16\'))") (constraint track_width (min 0.15mm)))')
 rules.append('(rule "USB connector escapes" (condition "A.Type == \'Track\' && A.intersectsCourtyard(\'J1\')") (constraint track_width (min 0.15mm)))')
 PROJECT.with_suffix('.kicad_dru').write_text('\n'.join(rules)+'\n')
 report={'source_date':'2026-10-05','board':'4-layer FR4 1.6mm, outer copper 1 oz, inner copper 0.5 oz; In2.Cu ground reference','classes':CLASSES,
 'net_assignments':{n.GetNetname():classify(n.GetNetname()) for n in b.GetNetsByName().values() if n.GetNetname()},
 'manufacturer_sources':['https://jlcpcb.com/capabilities/pcb-capabilities/','https://jlcpcb.com/capabilities/pcb-assembly-capabilities'],
 'note':'USB geometry is a routing target on a 4-layer board without a validated controlled-impedance stackup; no 90-ohm production guarantee.'}
 (ROOT/'outputs/routing-rules.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Defined',len(CLASSES),'netclasses and',len(report['net_assignments']),'nets')

if __name__=='__main__':write_project()
