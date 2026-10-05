#!/usr/bin/python3
"""Reproducible KiCad design. Pin maps, BOM and PCB share one source of truth."""
from pathlib import Path
import re, json, uuid, csv, math
import pcbnew as p

ROOT=Path(__file__).resolve().parents[1]
HW=ROOT/'hardware'; HW.mkdir(exist_ok=True)
LIB=HW/'Chiptune.pretty'; LIB.mkdir(exist_ok=True)
NAME='Chiptune_USB_CV'
def uid(s): return str(uuid.uuid5(uuid.NAMESPACE_URL,'chiptune-2026/'+s))
def q(s): return json.dumps(str(s),ensure_ascii=False)
parts=[]; counts={}
def add(ref,value,fp,nets,group,x,y,rot=0,lcsc='',pins=None,side='F',notes=''):
    obj=dict(ref=ref,value=value,fp=fp,nets={str(k):v for k,v in nets.items()},group=group,
             x=x,y=y,rot=rot,lcsc=lcsc,pins=pins,side=side,notes=notes,uuid=uid(ref))
    parts.append(obj); return obj
def passive(kind,value,a,b,group,x,y,rot=0,lcsc='',size='0603'):
    counts[kind]=counts.get(kind,0)+1; ref=f'{kind}{counts[kind]}'
    fp={'R':f'Resistor_SMD:R_{size}_1608Metric','C':f'Capacitor_SMD:C_{size}_1608Metric'}[kind]
    if size=='0805': fp=fp.replace('1608','2012')
    if size=='1206': fp=fp.replace('1608','3216')
    return add(ref,value,fp,{1:a,2:b},group,x,y,rot,lcsc,pins=[('1','1','passive'),('2','2','passive')])
def r(v,a,b,g,x,y,rot=0,c='',size='0603'): return passive('R',v,a,b,g,x,y,rot,c,size)
def c(v,a,b,g,x,y,rot=0,code='',size='0603'): return passive('C',v,a,b,g,x,y,rot,code,size)

# Coordinates are millimetres relative to a 85 x 54 mm board origin.
# Group 1: MCU, flash, oscillator, programming.
rptext=Path('/usr/share/kicad/symbols/MCU_RaspberryPi.kicad_sym').read_text().split('\n\t(symbol "RP2350')[0]
rppins=[(n,name,typ) for typ,name,n in re.findall(r'\(pin (\w+) \w+.*?\(name "([^"]*)".*?\(number "([^"]*)"',rptext,re.S)]
gpio={**{i:f'D{i}' for i in range(8)},8:'WR_N',9:'SAA_CS_N',10:'SN1_CS_N',11:'SN2_CS_N',12:'SAA_A0',
      13:'MIDI_RX',14:'SAA_CLK_RAW',15:'TYPEC_ID_N',16:'SN_CLK_RAW',17:'HOST_DISABLE',18:'CC_SDA',19:'CC_SCL',
      20:'CC_EN_N',21:'USB_FAULT_N',22:'MODE_BUTTON',23:'SN1_READY',24:'SN2_READY',25:'SAA_DTACK_N',
      26:'RAIL_ADC',27:'VBUS_PRESENT_N',28:'STATUS_LED',29:None}
rn={}
for num,nm,typ in rppins:
    if nm.startswith('GPIO'): rn[num]=gpio[int(re.search(r'GPIO(\d+)',nm)[1])]
    else: rn[num]={'TESTEN':'GND','RUN':'RUN','USB_DM':'USB_DM_MCU','USB_DP':'USB_DP_MCU',
      '~{QSPI_SS}':'FLASH_CS','QSPI_SCLK':'FLASH_CLK','QSPI_SD0':'FLASH_D0','QSPI_SD1':'FLASH_D1',
      'QSPI_SD2':'FLASH_D2','QSPI_SD3':'FLASH_D3','XIN':'XIN','XOUT':'XOUT',
      'SWCLK':'SWCLK','SWDIO':'SWDIO','USB_VDD':'+3V3','ADC_AVDD':'+3V3_A',
      'IOVDD':'+3V3','GND':'GND','VREG_VIN':'+3V3','VREG_VOUT':'+1V1','DVDD':'+1V1'}[nm]
add('U1','RP2040','Package_DFN_QFN:QFN-56-1EP_7x7mm_P0.4mm_EP3.2x3.2mm',rn,1,25,17,0,'C2040',rppins)
add('U2','W25Q16JVSSIQ','Package_SO:SOIC-8_5.3x5.3mm_P1.27mm',{1:'FLASH_CS',2:'FLASH_D1',3:'FLASH_D2',4:'GND',5:'FLASH_D0',6:'FLASH_CLK',7:'FLASH_D3',8:'+3V3'},1,15,17,90,'C131025',
    [('1','CS_N','input'),('2','IO1','bidirectional'),('3','IO2','bidirectional'),('4','GND','power_in'),('5','IO0','bidirectional'),('6','CLK','input'),('7','IO3','bidirectional'),('8','VCC','power_in')])
add('Y1','ABM8-272-T3 12MHz','Crystal:Crystal_SMD_3225-4Pin_3.2x2.5mm',{1:'XIN',2:'GND',3:'XTAL_OUT',4:'GND'},1,25,26,0,'C20625731',[(str(i),nm,'passive') for i,nm in enumerate(['XIN','CASE','XOUT','CASE'],1)])
r('1k','XOUT','XTAL_OUT',1,28.5,26,90,c='C21190')
c('15p C0G','XIN','GND',1,22,27,90,code='C1644'); c('15p C0G','XTAL_OUT','GND',1,28.5,29,90,code='C1644')
r('10k','FLASH_CS','+3V3',1,12,12,c='C25804'); r('10k','RUN','+3V3',1,35,17,90,c='C25804')
for x,y,rail in [(21,12,'+3V3'),(29,12,'+3V3'),(31,15,'+3V3'),(31,19,'+3V3'),(27,22,'+3V3'),(23,22,'+3V3'),(19,19,'+3V3'),(14,22,'+3V3'),(20,25,'+3V3_A'),(32,22,'+1V1'),(19,23,'+1V1')]:
    c('100n',rail,'GND',1,x,y,90,code='C14663')
c('1u','+3V3','GND',1,33,26,90,code='C15849'); c('1u','+1V1','GND',1,34,23,90,code='C15849')
r('10R','+3V3','+3V3_A',1,17,26,c='C22859')
add('SW2','BOOTSEL','Chiptune:TS1187A',{1:'BOOT_GND',2:'GND'},1,9,28,0,'C318884', [('1','1','passive'),('2','2','passive')])
r('1k','FLASH_CS','BOOT_GND',1,12,26,90,c='C21190')
add('SW3','RESET','Chiptune:TS1187A',{1:'RUN',2:'GND'},1,9,35,0,'C318884',[('1','1','passive'),('2','2','passive')])
add('J4','SWD','Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',{1:'+3V3',2:'SWDIO',3:'SWCLK',4:'GND'},1,4,40,0,'',[(str(i),nm,'passive') for i,nm in enumerate(['3V3','SWDIO','SWCLK','GND'],1)],notes='Hand solder, optional')

# Group 2: battery, reverse polarity, 5.1 V boost and 3.3 V LDO.
add('BT1','BH-AAA-B5AA001 2xAAA','Chiptune:BH-AAA-B5AA001',{1:'BAT_MID',2:'BAT_RAW',3:'BAT_MID',4:'GND'},2,40,40,0,'C964881',[('1','CELL1-','passive'),('2','CELL1+','power_out'),('3','CELL2+','passive'),('4','CELL2-','passive')],side='B',notes='Hand solder underside; verified manufacturer PCB drawing')
add('Q1','DMP2035U-7','Package_TO_SOT_SMD:SOT-23',{1:'GND',2:'BAT_PROT',3:'BAT_RAW'},2,12,44,0,'C110499',[('1','G','input'),('2','S','passive'),('3','D','passive')])
add('U3','TPS61023DRLR','Package_TO_SOT_SMD:SOT-563',{1:'BOOST_FB',2:'BOOST_EN',3:'BAT_PROT',4:'GND',5:'BOOST_SW',6:'+5V'},2,21,43,0,'C919459',
    [('1','FB','input'),('2','EN','input'),('3','VIN','power_in'),('4','GND','power_in'),('5','SW','passive'),('6','VOUT','power_out')])
add('U4','XC61CC2002MR-G','Package_TO_SOT_SMD:SOT-23',{1:'BAT_OK',2:'GND',3:'BAT_PROT'},2,12,49,0,'C2461421',[('1','VOUT','output'),('2','VSS','power_in'),('3','VIN','power_in')])
add('SW1','POWER MSK12C02','Button_Switch_SMD:SW_SPDT_Shouhan_MSK12C02',{1:'GND',2:'BOOST_EN',3:'BAT_OK','SH':'GND'},2,20,49.5,0,'C431540',[(str(i),str(i),'passive') for i in range(1,4)]+[('SH','SHELL','passive')])
r('100k','BOOST_EN','GND',2,17,48,90,c='C25803')
add('L1','SLO0520H1R0MTT 1uH','Chiptune:SLO0520H',{1:'BAT_PROT',2:'BOOST_SW'},2,21,38,0,'C325980', [('1','1','passive'),('2','2','passive')],notes='1uH; Isat 8A typ, Irms 5A typ; manufacturer drawing verified')
c('22u 25V','BAT_PROT','GND',2,16,38,90,code='C45783',size='0805')
c('22u 25V','BAT_PROT','GND',2,16,41,90,code='C45783',size='0805')
c('22u 25V','+5V','GND',2,24,43,90,code='C45783',size='0805')
c('22u 25V','+5V','GND',2,27,43,90,code='C45783',size='0805')
c('100n','BAT_PROT','GND',2,14.5,46,90,code='C14663')
r('22k','+5V','BOOST_FB',2,23,47,90,c='C31850'); r('3k','BOOST_FB','GND',2,26,47,90,c='C4211')
add('U5','AP2112K-3.3TRG1','Package_TO_SOT_SMD:SOT-23-5',{1:'+5V',2:'GND',3:'+5V',4:None,5:'+3V3'},2,33,40,0,'C51118',
    [('1','VIN','power_in'),('2','GND','power_in'),('3','EN','input'),('4','NC','no_connect'),('5','VOUT','power_out')])
c('4.7u 16V','+5V','GND',2,31,36,90,code='C19666'); c('4.7u 16V','+3V3','GND',2,35,40,90,code='C19666')
# Sense the enabled rail rather than raw cells to avoid ADC back-power when off.
r('100k','+5V','RAIL_ADC',2,32,45,90,c='C25803'); r('100k','RAIL_ADC','GND',2,35,45,90,c='C25803')

# Group 3: USB-C DRP, current-limited host power and ESD.
usbfp='Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12'
usbnet={**{n:'GND' for n in ['A1','A12','B1','B12','S1']},**{n:'USB_VBUS' for n in ['A4','A9','B4','B9']},'A5':'CC1','B5':'CC2','A6':'USB_DP','B6':'USB_DP','A7':'USB_DM','B7':'USB_DM','A8':None,'B8':None}
add('J1','USB-C MIDI DRP',usbfp,usbnet,3,11,6.3,0,'C165948',[(n,n,'passive') for n in usbnet])
add('U6','TUSB320LAIRWBR','Package_DFN_QFN:Texas_X2QFN-12_1.6x1.6mm_P0.4mm',{1:'CC1',2:'CC2',3:None,4:'VBUS_DET',5:'GND',6:None,7:'CC_SDA',8:'CC_SCL',9:'TYPEC_ID_N',10:'GND',11:'CC_EN_N',12:'+3V3'},3,7,12,0,'C132554',
    [('1','CC1','bidirectional'),('2','CC2','bidirectional'),('3','PORT_DRP_NC','input'),('4','VBUS_DET','input'),('5','ADDR_0x47','input'),('6','INT_N','open_collector'),('7','SDA','bidirectional'),('8','SCL','input'),('9','ID_N','open_collector'),('10','GND','power_in'),('11','EN_N','input'),('12','VDD','power_in')])
add('U7','TPS2552DBVR','Package_TO_SOT_SMD:SOT-23-6',{1:'+5V',2:'GND',3:'HOST_EN_N',4:'USB_FAULT_N',5:'USB_ILIM',6:'USB_VBUS'},3,38,8,0,'C46506',
    [('1','IN','power_in'),('2','GND','power_in'),('3','EN_N','input'),('4','FAULT_N','open_collector'),('5','ILIM','passive'),('6','OUT','power_out')])
add('U8','74LVC1G32GW,125','Package_TO_SOT_SMD:SOT-353_SC-70-5',{1:'TYPEC_ID_N',2:'HOST_DISABLE',3:'GND',4:'HOST_EN_N',5:'+3V3'},3,33,8,0,'C12516',
    [('1','A','input'),('2','B','input'),('3','GND','power_in'),('4','Y','output'),('5','VCC','power_in')])
add('U9','USBLC6-2SC6','Package_TO_SOT_SMD:SOT-23-6',{1:'USB_DP',2:'GND',3:'USB_DM',4:'USB_DM',5:'USB_VBUS',6:'USB_DP'},3,13,8,0,'C7519',[(str(i),nm,'passive') for i,nm in enumerate(['IO1','GND','IO2','IO2','VBUS','IO1'],1)])
r('27R','USB_DP','USB_DP_MCU',3,18,9,c='C25190'); r('27R','USB_DM','USB_DM_MCU',3,18,7,c='C25190')
r('910k 1%','USB_VBUS','VBUS_DET',3,4,15,90,c='C54531625')
r('220k','TYPEC_ID_N','+3V3',3,4,11,90,c='C22961')
r('4.7k','CC_SDA','+3V3',3,7,16,90,c='C23162'); r('4.7k','CC_SCL','+3V3',3,9.5,16,90,c='C23162')
r('100k','CC_EN_N','+3V3',3,7,19,90,c='C25803'); r('100k','HOST_DISABLE','+3V3',3,32,5,c='C25803')
r('10k','USB_FAULT_N','+3V3',3,41,8,90,c='C25804')
r('47k','USB_ILIM','GND',3,41,11,90,c='C25819')
r('100k','USB_VBUS','VBUS_GATE',3,37,13,90,c='C25803'); r('470k','VBUS_GATE','GND',3,40,14,90,c='C23178')
r('10k','VBUS_PRESENT_N','+3V3',3,37,16,90,c='C25804')
add('Q2','2N7002','Package_TO_SOT_SMD:SOT-23',{1:'VBUS_GATE',2:'GND',3:'VBUS_PRESENT_N'},3,40,18,0,'C8545',[('1','G','input'),('2','S','passive'),('3','D','open_collector')])
r('100k','HOST_EN_N','+5V',3,33,17,90,c='C25803')
c('100n','+3V3','GND',3,7,8,90,code='C14663'); c('100n','+3V3','GND',3,30,8,90,code='C14663'); c('1u','+5V','GND',3,38,5,code='C15849')
# Host reservoir placed before switch; <=10uF remains on external VBUS in device mode.
add('C'+str(counts['C']+1),'100u 16V','Capacitor_SMD:CP_Elec_6.3x5.4',{1:'+5V',2:'GND'},3,34,12,90,'C2693600',[('1','+','passive'),('2','-','passive')]); counts['C']+=1; c('4.7u 16V','USB_VBUS','GND',3,43,6,90,code='C19666')
add('SW4','USB ROLE / PANIC','Chiptune:TS1187A',{1:'MODE_BUTTON',2:'GND'},3,38,23,0,'C318884',[('1','1','passive'),('2','2','passive')])
r('10k','MODE_BUTTON','+3V3',3,38,20,90,c='C25804')

# Group 4: real sound chips, common data bus, independent selects.
saapins=[('1','WR_N','input'),('2','CS_N','input'),('3','A0','input'),('4','OUT_R','output'),('5','OUT_L','output'),('6','IREF','passive'),('7','DTACK_N','open_collector'),('8','CLK','input'),('9','VSS','power_in')]+[(str(10+i),f'D{i}','input') for i in range(8)]+[('18','VDD','power_in')]
snmap={1:'D2',2:'D1',3:'D0',4:'READY',5:'WE_N',6:'CE_N',7:'AUDIO',8:'GND',9:'NC',10:'D7',11:'D6',12:'D5',13:'D4',14:'CLK',15:'D3',16:'VCC'}
saan={1:'WR_N',2:'SAA_CS_N',3:'SAA_A0',4:'SAA_R',5:'SAA_L',6:'SAA_IREF',7:'SAA_DTACK_N',8:'SAA_CLK',9:'GND',18:'+5V',**{10+i:f'D{i}' for i in range(8)}}
add('U10','SAA1099 DIP18','Package_DIP:DIP-18_W7.62mm_Socket',saan,4,51,7,0,'',saapins,notes='User supplied IC; hand solder DIP18 socket')
for k,x in [(1,64),(2,75)]:
    snn={num: (f'SN{k}_READY' if nm=='READY' else f'SN{k}_CS_N' if nm=='CE_N' else f'SN{k}_OUT' if nm=='AUDIO' else 'WR_N' if nm=='WE_N' else '+5V' if nm=='VCC' else 'SN_CLK' if nm=='CLK' else None if nm=='NC' else nm) for num,nm in snmap.items()}
    snpins=[(str(n),nm,'power_in' if nm in ['GND','VCC'] else 'open_collector' if nm=='READY' else 'output' if nm=='AUDIO' else 'no_connect' if nm=='NC' else 'input') for n,nm in snmap.items()]
    add(f'U{10+k}','SN76489AN DIP16','Package_DIP:DIP-16_W7.62mm_Socket',snn,4,x,7,0,'',snpins,notes='User supplied IC; hand solder DIP16 socket')
r('33R','SAA_CLK_RAW','SAA_CLK',4,47,10,90,c='C23140'); r('33R','SN_CLK_RAW','SN_CLK',4,47,14,90,c='C23140')
for i,(net,x,y) in enumerate([('WR_N',43,20),('SAA_CS_N',43,23),('SN1_CS_N',43,26),('SN2_CS_N',43,29),('SAA_DTACK_N',47,26),('SN1_READY',62,30),('SN2_READY',76,30)]):
    r('10k',net,'+3V3',4,x,y,90,c='C25804')
r('100k','SAA_A0','GND',4,47,22,90,c='C25803')
r('100k','SAA_CLK_RAW','GND',4,43,32,90,c='C25803'); r('100k','SN_CLK_RAW','GND',4,47,32,90,c='C25803')
for x,y in [(61,7),(74,7),(47,6)]: c('100n','+5V','GND',4,x,y,90,code='C14663')
for x,y in [(61,11),(74,11),(47,18)]: c('4.7u 16V','+5V','GND',4,x,y,90,code='C19666')

# Group 5: isolated TRS Type A MIDI input. Sleeve and jack switch are not grounded.
add('J2','TRS MIDI IN TYPE A','Chiptune:PJ320D4A',{1:None,2:'MIDI_TIP',3:'MIDI_RING',4:None},5,76.7,34.5,180,'C95562', [('1','SLEEVE_ISOLATED','passive'),('2','TIP','passive'),('3','RING','passive'),('4','RING2_NC','passive')])
r('220R','MIDI_RING','MIDI_ANODE',5,65,35,c='C22962')
add('D1','1N4148W','Diode_SMD:D_SOD-123',{1:'MIDI_ANODE',2:'MIDI_TIP'},5,65,39,0,'C81598',[('1','K','passive'),('2','A','passive')])
add('U13','6N137S(T1)','Package_DIP:SMDIP-8_W9.53mm',{1:None,2:'MIDI_ANODE',3:'MIDI_TIP',4:None,5:'GND',6:'MIDI_RX',7:'+5V',8:'+5V'},5,56,38,0,'C191867',
    [('1','NC','no_connect'),('2','ANODE','passive'),('3','CATHODE','passive'),('4','NC','no_connect'),('5','GND','power_in'),('6','OUT','open_collector'),('7','ENABLE','input'),('8','VCC','power_in')])
r('2.2k','MIDI_RX','+3V3',5,52,42,90,c='C4190'); c('100n','+5V','GND',5,51,37,90,code='C14663')

# Group 6: stereo line mixer; SAA current outputs need pull-up loads.
add('U14','TLV9004IPWR','Package_SO:TSSOP-14_4.4x5mm_P0.65mm',
    {1:'VREF',2:'VREF',3:'VREF_DIV',4:'+5V_A',5:'VREF',6:'SUM_L',7:'MIX_L',8:'MIX_R',9:'SUM_R',10:'VREF',11:'GND',12:'VREF',13:'UNUSED_OP_OUT',14:'UNUSED_OP_OUT'},6,46,43,0,'C2058050',
    [('1','OUT_A','output'),('2','IN-_A','input'),('3','IN+_A','input'),('4','VCC','power_in'),('5','IN+_B','input'),('6','IN-_B','input'),('7','OUT_B','output'),('8','OUT_C','output'),('9','IN-_C','input'),('10','IN+_C','input'),('11','VSS','power_in'),('12','IN+_D','input'),('13','IN-_D','input'),('14','OUT_D','output')])
add('FB1','10R analog RC filter','Resistor_SMD:R_0603_1608Metric',{1:'+5V',2:'+5V_A'},6,39,43,0,'C22859',[('1','1','passive'),('2','2','passive')])
c('22u 25V','+5V_A','GND',6,39,47,90,code='C45783',size='0805'); c('100n','+5V_A','GND',6,43,39,90,code='C14663')
r('10k','+5V_A','VREF_DIV',6,42,47,90,c='C25804'); r('10k','VREF_DIV','GND',6,45,47,90,c='C25804'); c('4.7u 16V','VREF_DIV','GND',6,42,51,code='C19666')
r('10k','+5V','SAA_IREF',6,54,30,c='C25804'); r('1.5k','+5V','SAA_L',6,50,30,90,c='C22843'); r('1.5k','+5V','SAA_R',6,58,30,90,c='C22843')
for src,mix,summ,x,y in [('SAA_L','L','SUM_L',51,46),('SN1_OUT','L','SUM_L',57,46),('SAA_R','R','SUM_R',51,50),('SN2_OUT','R','SUM_R',57,50)]:
    ac=src+'_AC'; c('1u',src,ac,6,x,y,90,code='C15849'); r('22k',ac,summ,6,x+3,y,90,c='C31850')
for ch,x,y in [('L',47,49),('R',47,52)]:
    r('10k','MIX_'+ch,'SUM_'+ch,6,x,y,c='C25804'); c('470p X7R','MIX_'+ch,'SUM_'+ch,6,x+3,y,code='C1620')
    c('10u 10V','MIX_'+ch,'LINE_'+ch+'_AC',6,64,y,90,code='C19702')
    r('100R','LINE_'+ch+'_AC','LINE_'+ch,6,68,y,90,c='C22775'); r('100k','LINE_'+ch,'GND',6,71,y,90,c='C25803')
add('J3','STEREO LINE OUT','Chiptune:PJ320D4A',{1:'GND',2:'LINE_L',3:'LINE_R',4:None},6,76.7,44.5,180,'C95562',[('1','SLEEVE','passive'),('2','TIP_L','passive'),('3','RING_R','passive'),('4','RING2_NC','passive')])
# Status and spare ADC: GPIO26 senses 5V health in this revision, not raw battery.
rn['38']='RAIL_ADC'
add('D2','GREEN STATUS','LED_SMD:LED_0603_1608Metric',{1:'GND',2:'LED_A'},1,35,29,0,'C72043',[('1','K','passive'),('2','A','passive')])
r('2.2k','STATUS_LED','LED_A',1,35,32,90,c='C4190')

from usb_variant import configure
configure(globals())

def vec(x,y): return p.VECTOR2I(p.FromMM(x),p.FromMM(y))
def write_custom_fp(name,w,h,pads,side='F'):
    out=f'(footprint {q(name)} (version 20241229) (generator "pcbnew") (layer "F.Cu") (attr smd)\n'
    out+=f'(property "Reference" "REF**" (at 0 {-h/2-1.5}) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    out+=f'(property "Value" {q(name)} (at 0 {h/2+1.5}) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    for layer,d in [('F.Fab',0),('F.CrtYd',0.5)]:
        out+=f'(fp_rect (start {-w/2-d} {-h/2-d}) (end {w/2+d} {h/2+d}) (stroke (width 0.05) (type default)) (fill none) (layer "{layer}"))\n'
    for n,x,y,pw,ph in pads:
        out+=f'(pad {q(n)} smd rect (at {x} {y}) (size {pw} {ph}) (layers "F.Cu" "F.Paste" "F.Mask"))\n'
    out+=')\n'; (LIB/(name+'.kicad_mod')).write_text(out)

def build_board():
    board=p.BOARD(); board.SetCopperLayerCount(2)
    nc=board.GetAllNetClasses()['Default']; nc.SetClearance(p.FromMM(.15)); nc.SetTrackWidth(p.FromMM(.15)); nc.SetViaDiameter(p.FromMM(.6)); nc.SetViaDrill(p.FromMM(.3))
    board.GetDesignSettings().m_MinClearance=p.FromMM(.15)
    allnets=sorted({net for obj in parts for net in obj['nets'].values() if net})
    nets={}
    for name in allnets:
        net=p.NETINFO_ITEM(board,name); board.Add(net); nets[name]=net
    for obj in parts:
        lib,name=obj['fp'].split(':')
        libpath=LIB if lib=='Chiptune' else Path('/usr/share/kicad/footprints')/(lib+'.pretty')
        fp=p.FootprintLoad(str(libpath),name)
        if fp is None: raise ValueError(obj['ref']+' missing footprint '+obj['fp'])
        fp.SetReference(obj['ref']); fp.SetValue(obj['value']); fp.SetFPID(p.LIB_ID(lib,name))
        fp.SetPosition(vec(100+obj['x'],100+obj['y'])); fp.SetOrientationDegrees(obj['rot'])
        # Flip only after assigning the board parent.
        fp.SetPath(p.KIID_PATH('/'+uid(NAME)+'/'+uid('hier'+str(obj['group']))+'/'+obj['uuid']))
        fp.SetLibDescription(obj['notes'])
        board.Add(fp)
        if obj['side']=='B': fp.Flip(fp.GetPosition(),False)
        for pad in fp.Pads():
            n=pad.GetNumber()
            if pad.GetAttribute()==p.PAD_ATTRIB_NPTH : continue
            if n and n not in obj['nets']: raise ValueError(obj['ref']+' unmapped pad '+n)
            if n and obj['nets'][n]: pad.SetNet(nets[obj['nets'][n]])
        # Keep assembly values on Fab; small references will be adjusted after routing.
        fp.Reference().SetTextSize(vec(.8,.8)); fp.Reference().SetTextThickness(p.FromMM(.12))
        fp.Value().SetVisible(False)
    corners=[(100,100),(185,100),(185,154),(100,154)]
    for a,b in zip(corners,corners[1:]+corners[:1]):
        edge=p.PCB_SHAPE(); edge.SetShape(p.SHAPE_T_SEGMENT); edge.SetLayer(p.Edge_Cuts)
        edge.SetStart(vec(*a)); edge.SetEnd(vec(*b)); edge.SetWidth(p.FromMM(.05)); board.Add(edge)
    for ref,x,y in [('H1',3,3),('H2',82,3),('H3',82,51),('H4',3,51)]:
        fp=p.FootprintLoad('/usr/share/kicad/footprints/MountingHole.pretty','MountingHole_2.2mm_M2')
        fp.SetReference(ref); fp.SetPosition(vec(100+x,100+y)); board.Add(fp)
    for text,x,y,size in [('CHIPTUNE / 2AAA',28,3,1.1),('SAA1099',55,4,.8),('SN1',68,4,.8),('SN2',80,4,.8),('USB-C / DRP',23,6,.8),('MIDI A',79,32,.8),('LINE OUT',78,42,.8),('REV C - PLACEMENT DRAFT',30,50,.8)]:
        t=p.PCB_TEXT(board); t.SetText(text); t.SetPosition(vec(100+x,100+y)); t.SetLayer(p.F_SilkS)
        t.SetTextSize(vec(size,size)); t.SetTextThickness(p.FromMM(.12)); board.Add(t)
    p.SaveBoard(str(HW/(NAME+'.kicad_pcb')),board)
    return board

def symbol_def(obj):
    name=obj['ref']; pins=obj['pins']; n=len(pins); half=math.ceil(n/2)
    # Passive two-pin parts use familiar R/C geometry and horizontal pins.
    if name.startswith(('R','C')) and len(pins)==2:
        coords=[(-6.35,0,0),(6.35,0,180)]; height=2.54; width=2.54
        if name.startswith('C'):
            graphic=''.join(f'(polyline (pts (xy {x} -2.54) (xy {x} 2.54)) (stroke (width 0.25) (type default)) (fill (type none)))' for x in [-.64,.64])
            graphic+=''.join(f'(polyline (pts (xy {a} 0) (xy {b} 0)) (stroke (width 0.15) (type default)) (fill (type none)))' for a,b in [(-2.54,-.64),(.64,2.54)])
        else: graphic='(rectangle (start -2.54 1.27) (end 2.54 -1.27) (stroke (width 0.25) (type default)) (fill (type background)))'
    else:
        height=max(5.08,(half+1)*2.54); width=12.7 if n>8 else 8.89
        coords=[]
        for i in range(n):
            if i<half: coords.append((-width-3.81,height/2-2.54-i*2.54,0))
            else: coords.append((width+3.81,height/2-2.54-(i-half)*2.54,180))
        graphic=f'(rectangle (start {-width} {height/2}) (end {width} {-height/2}) (stroke (width 0.25) (type default)) (fill (type background)))'
    body=f'(symbol {q(name)} (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)'
    for prop,val in [('Reference',name),('Value',obj['value']),('Footprint',obj['fp']),('Datasheet','https://www.lcsc.com/datasheet/'+obj['lcsc']+'.pdf' if obj['lcsc'] else ''),('LCSC',obj['lcsc'])]:
        body+=f'(property {q(prop)} {q(val)} (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
    body+=f'(symbol {q(name+"_0_1")} {graphic}) (symbol {q(name+"_1_1")}'
    for (num,nm,typ),(x,y,angle) in zip(pins,coords):
        body+=f'(pin {typ} line (at {x} {y} {angle}) (length 3.81) (name {q(nm)} (effects (font (size 1 1)))) (number {q(num)} (effects (font (size 1 1)))))'
    body+='))'; return body,coords,height

def build_schematics():
    groups={1:'MCU and programming',2:'USB power and sound supply',3:'USB-C device',4:'Sound IC sockets',5:'Isolated MIDI input',6:'Stereo line mixer',7:'CV and gate inputs',8:'OLED and controls'}
    defs={obj['ref']:symbol_def(obj) for obj in parts}
    (HW/'Chiptune.kicad_sym').write_text('(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor")\n'+''.join(d[0] for d in defs.values())+'(symbol \"PWR_FLAG\" (power) (in_bom no) (on_board no) (property \"Reference\" \"#FLG\" (at 0 0 0) (effects (font (size 1 1)) (hide yes))) (property \"Value\" \"PWR_FLAG\" (at 0 2.54 0) (effects (font (size 1 1)))) (symbol \"PWR_FLAG_0_1\" (pin power_out line (at 0 0 90) (length 0) (name \"pwr\" (effects (font (size 1 1)))) (number \"1\" (effects (font (size 1 1))))))))\n')
    for group,title in groups.items():
        groupobjs=[o for o in parts if o['group']==group]; sheetid=uid(f'sheet{group}')
        s=f'(kicad_sch (version 20250114) (generator "eeschema") (uuid {sheetid}) (paper "A2")'
        s+=f'(title_block (title {q(title)}) (date "2026-10-05") (rev "C - PLACEMENT DRAFT") (comment 1 "Chiptune USB / CV - 85x54mm"))'
        s+='(lib_symbols '+''.join(d[0].replace('(symbol '+q(ref), '(symbol '+q('Chiptune:'+ref),1) for ref,d in defs.items() if any(o['ref']==ref for o in groupobjs))+('(symbol \"Chiptune:PWR_FLAG\" (power) (in_bom no) (on_board no) (property \"Reference\" \"#FLG\" (at 0 0 0) (effects (font (size 1 1)) (hide yes))) (property \"Value\" \"PWR_FLAG\" (at 0 2.54 0) (effects (font (size 1 1)))) (symbol \"PWR_FLAG_0_1\" (pin power_out line (at 0 0 90) (length 0) (name \"pwr\" (effects (font (size 1 1)))) (number \"1\" (effects (font (size 1 1)))))))' if group==1 else '')+')'
        cx=0; cy=0; rowheight=0
        for idx,obj in enumerate(groupobjs):
            definition,coords,h=defs[obj['ref']]
            boxw=100; boxh=max(25,h+16)
            if cx+boxw>560: cx=0; cy+=rowheight; rowheight=0
            x=round((45+cx)/1.27)*1.27; y=round((24+cy+boxh/2)/1.27)*1.27; cx+=boxw; rowheight=max(rowheight,boxh)
            if y+boxh/2>385: raise ValueError('Schematic sheet overflow '+title)
            ref=obj['ref']; symuid=obj['uuid']
            s+=f'(symbol (lib_id {q("Chiptune:"+ref)}) (at {x} {y} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid {symuid})'
            for prop,val,py,hide in [('Reference',ref,y-h/2-5,False),('Value',obj['value'],y-h/2-2,False),('Footprint',obj['fp'],y,True),('Datasheet','',y,True),('LCSC',obj['lcsc'],y,True)]:
                s+=f'(property {q(prop)} {q(val)} (at {x} {py} 0) (effects (font (size 1.1 1.1))'+(' (hide yes)' if hide else '')+'))'
            for num,nm,typ in obj['pins']: s+=f'(pin {q(num)} (uuid {uid(ref+"/pin"+num)}))'
            s+=f'(instances (project {q(NAME)} (path "/{uid(NAME)}/{uid("hier"+str(group))}" (reference {q(ref)}) (unit 1)))))'
            for (num,nm,typ),(dx,dy,angle) in zip(obj['pins'],coords):
                px,py=x+dx,y-dy; net=obj['nets'][num]
                if net is None:
                    s+=f'(no_connect (at {px} {py}) (uuid {uid(ref+"nc"+num)}))'; continue
                lx=px-5.08 if angle==0 else px+5.08
                s+=f'(wire (pts (xy {px} {py}) (xy {lx} {py})) (stroke (width 0) (type default)) (uuid {uid(ref+"wire"+num)}))'
                # Global labels join repeated nets across the child sheets.
                s+=f'(global_label {q(net)} (shape bidirectional) (at {lx} {py} {0 if angle==0 else 180}) (effects (font (size 1 1)) (justify {"left" if angle==0 else "right"})) (uuid {uid(ref+"label"+num)}))'
        if group==1:
            for i,net in enumerate(['GND','USB_VBUS','+3V2_A','+5V_A']):
                fx=round((30+60*i)/1.27)*1.27; fy=360.68; fr=f'#FLG0{i+1}'
                s+=f'(symbol (lib_id "Chiptune:PWR_FLAG") (at {fx} {fy} 0) (unit 1) (in_bom no) (on_board no) (uuid {uid(fr)}) (property "Reference" "{fr}" (at {fx} {fy} 0) (effects (font (size 1 1)) (hide yes))) (property "Value" "PWR_FLAG" (at {fx} {fy-2.54} 0) (effects (font (size 1 1)))) (instances (project "{NAME}" (path "/{uid(NAME)}/{uid("hier1")}" (reference "{fr}") (unit 1)))))'
                s+=f'(global_label {q(net)} (shape bidirectional) (at {fx} {fy} 0) (effects (font (size 1 1)) (justify left)) (uuid {uid(fr+"net")}))'
        s+=f'(sheet_instances (path "/" (page "{group+1}"))) (embedded_fonts no))'
        (HW/f'{group:02d}.kicad_sch').write_text(s)
    root=f'(kicad_sch (version 20250114) (generator "eeschema") (uuid {uid(NAME)}) (paper "A4") (title_block (title "CHIPTUNE USB-CV / SAA1099 + 2xSN76489AN") (date "2026-10-05") (rev "C - PLACEMENT DRAFT")) (lib_symbols)'
    for g,title in groups.items():
        x=20+((g-1)%2)*125; y=28+((g-1)//2)*42
        root+=f'(sheet (at {x} {y}) (size 105 35) (stroke (width 0.25) (type default)) (fill (color 0 0 0 0)) (uuid {uid("hier"+str(g))}) (property "Sheetname" {q(title)} (at {x} {y-2} 0) (effects (font (size 1.27 1.27)) (justify left))) (property "Sheetfile" "{g:02d}.kicad_sch" (at {x} {y+37} 0) (effects (font (size 1.27 1.27)) (justify left))) (instances (project {q(NAME)} (path "/{uid(NAME)}" (page "{g+1}")))))'
    root+='(sheet_instances (path "/" (page "1"))) (embedded_fonts no))'; (HW/(NAME+'.kicad_sch')).write_text(root)
    (HW/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "Chiptune") (type "KiCad") (uri "${KIPRJMOD}/Chiptune.kicad_sym") (options "") (descr "Project pin maps")))')
    (HW/'fp-lib-table').write_text('(fp_lib_table (version 7) (lib (name "Chiptune") (type "KiCad") (uri "${KIPRJMOD}/Chiptune.pretty") (options "") (descr "Custom package footprints")))')

def bom():
    (HW/'design.json').write_text(json.dumps(parts,indent=2))
    with (ROOT/'outputs'/'engineering_bom.csv').open('w') as f:
        wr=csv.writer(f); wr.writerow(['Reference','Value','Footprint','LCSC','Side','Notes'])
        for obj in parts: wr.writerow([obj[k] for k in ['ref','value','fp','lcsc','side','notes']])
    with (ROOT/'firmware'/'pinmap.h').open('w') as f:
        f.write('#pragma once\n// Generated from scripts/design.py; RP2040 GPIO assignments\n')
        for n,net in gpio.items():
            if net: f.write(f'#define PIN_{net} {n}\n')

if __name__=='__main__':
    # These two custom footprints remain explicitly flagged until drawing verification.
    write_custom_fp('BH-AAA-B5AA001',62.2,22.4,[(1,-27.875,-5.5,7.45,6.1),(2,27.875,-5.5,7.45,6.1),(3,-27.875,5.5,7.45,6.1),(4,27.875,5.5,7.45,6.1)])
    write_custom_fp('SLO0520H',5.4,5.1,[(1,-2.25,0,1.5,2.5),(2,2.25,0,1.5,2.5)])
    write_custom_fp('TS1187A',7,5.1,[(1,-3,-1.875,1,.75),(1,3,-1.875,1,.75),(2,-3,1.875,1,.75),(2,3,1.875,1,.75)])
    buttonfp=LIB/'TS1187A.kicad_mod'
    buttonfp.write_text(buttonfp.read_text().replace('(fp_rect (start -3.5 -2.55) (end 3.5 2.55)', '(fp_rect (start -2.55 -2.55) (end 2.55 2.55)'))
    bfp=LIB/'BH-AAA-B5AA001.kicad_mod'
    bs=bfp.read_text(); bs=bs.rstrip()[:-1]+'(pad "" np_thru_hole circle (at -23.5 -10.45) (size 1.8 1.8) (drill 1.8) (layers "*.Cu" "*.Mask"))\n)\n'; bfp.write_text(bs)
    jack=Path('/usr/share/kicad/footprints/Connector_Audio.pretty/Jack_3.5mm_KoreanHropartsElec_PJ-320D-4A_Horizontal.kicad_mod').read_text()
    jack=jack.replace('(footprint "Jack_3.5mm_KoreanHropartsElec_PJ-320D-4A_Horizontal"','(footprint "PJ320D4A"')
    for old,new in [('S','1'),('T','2'),('R1','3'),('R2','4')]: jack=jack.replace('(pad "'+old+'"','(pad "'+new+'"')
    (LIB/'PJ320D4A.kicad_mod').write_text(jack)
    from usb_footprints import write_interfaces
    write_interfaces(LIB)
    build_schematics(); build_board(); bom()
    print(f'Generated {len(parts)} components in {HW}')
