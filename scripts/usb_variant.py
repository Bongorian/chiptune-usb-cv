"""Apply USB-only/CV/UI revision to the stable sound-engine reference design."""
def configure(d):
    parts=d['parts']; add=d['add']; gpio=d['gpio']; rn=d['rn']; rppins=d['rppins']
    remove={'BT1','Q1','Q2','U4','U6','U7','U8','SW1','SW2','SW3','SW4','R9','R10','R13','R14','R15','R18','R19','R20','R21','R22','R23','R24','R25','R33','R34','C17','C20','C23','C24','C25','C26','C27'}
    parts[:]=[o for o in parts if o['ref'] not in remove]
    by={o['ref']:o for o in parts}
    gpio.update({15:'GATE1_N',17:'GATE2_N',18:'UI_SDA',19:'UI_SCL',20:'UI_MINUS',21:'UI_OK',22:'UI_PLUS',23:None,24:None,26:'CC1_ADC',27:'CC2_ADC',29:'SOUND_ENABLE'})
    gpio.update({i:f'D{7-i}' for i in range(8)})
    for n,nm,t in rppins:
        if nm.startswith('GPIO'):
            import re
            rn[n]=gpio[int(re.search(r'GPIO(\d+)',nm)[1])]
    by['U1']['nets']=rn.copy()
    # Align the socket and GPIO bit order; whole-port GPIO writes use logical bytes.
    by['C1']['nets']['1']='XTAL_OUT'
    by['C2']['nets']['1']='XIN'
    by['U10']['nets'].update({str(10+i):f'D{7-i}' for i in range(8)})
    by['U10']['notes']+='; GPIO0..7 word equals logical byte; reverse8 only for individual PIN_Dn writes'
    by['U3']['nets'].update({'2':'SOUND_ENABLE','3':'USB_VBUS'})
    by['L1']['nets']['1']='USB_VBUS'
    by['R6']['nets']['1']='SOUND_ENABLE'
    by['C16'].update(value='1u',fp='Capacitor_SMD:C_0603_1608Metric',lcsc='C15849',nets={'1':'USB_VBUS','2':'GND'})
    by['U5'].update(value='AP2120N-3.2TRG1',fp='Package_TO_SOT_SMD:SOT-23',lcsc='C6500828',nets={'1':'GND','2':'+3V3','3':'USB_VBUS'},pins=[('1','GND','power_in'),('2','VOUT','power_out'),('3','VIN','power_in')],notes='3.2V +/-2%; OLED maximum 3.3V')
    by['C21']['nets']['1']='USB_VBUS'
    by['J1']['value']='USB-C MIDI DEVICE / POWER'
    for r,n in [('R16','UI_SDA'),('R17','UI_SCL')]: by[r]['nets']['1']=n;by[r]['group']=8;by[r]['value']='4.7k';by[r]['lcsc']='C23162'
    for r in ['R28','R29','R30','R31']:by[r].update(value='100k',lcsc='C25803');by[r]['nets']['2']='GND'
    for r in ['U11','U12']:by[r]['nets']['4']=None;by[r]['notes']+='; READY unused, firmware write wait >=10us at 4MHz'
    for o in parts:o['nets']={n:{'+3V3':'+3V2','+3V3_A':'+3V2_A'}.get(v,v) for n,v in o['nets'].items()}
    def res(ref,val,a,b,x,y,code='C25804',g=7):
        return add(ref,val,'Resistor_SMD:R_0603_1608Metric',{1:a,2:b},g,x,y,0,code,[('1','1','passive'),('2','2','passive')])
    def cap(ref,a,b,x,y,val='100n',code='C14663',g=7):
        return add(ref,val,'Capacitor_SMD:C_0603_1608Metric',{1:a,2:b},g,x,y,0,code,[('1','1','passive'),('2','2','passive')])
    for k,y in [(1,46),(2,43.5)]:
        res(f'R{57+k}','5.1k',f'CC{k}','GND',25,y,'C23186',3)
        res(f'R{61+2*(k-1)}','100k',f'CC{k}',f'CC{k}_ADC',21,y,'C25803',3)
        res(f'R{62+2*(k-1)}','100k',f'CC{k}_ADC','GND',18.5,y,'C25803',3)
        cap(f'C{49+k}',f'CC{k}_ADC','GND',15.5,y,g=3)
    cap('C52','USB_VBUS','GND',27,47,'1u','C15849',3)
    # CV buffers: offset-biased 1/11 attenuators, differential reference cancels bias.
    opins=by['U14']['pins']
    add('U15','TLV9004IPWR CV BUFFER','Package_SO:TSSOP-14_4.4x5mm_P0.65mm',{1:'CV_BIAS',2:'CV_BIAS',3:'CV_BIAS_DIV',4:'+3V2',5:'CV1_NODE',6:'CV1_BUF',7:'CV1_BUF',8:'CV2_BUF',9:'CV2_BUF',10:'CV2_NODE',11:'GND',12:'CV_REF_NODE',13:'CV_REF',14:'CV_REF'},7,21,39,0,'C2058050',opins)
    add('U16','ADS1115IDGSR','Package_SO:VSSOP-10_3x3mm_P0.5mm',{1:'GND',2:None,3:'GND',4:'CV1_BUF',5:'CV_REF',6:'CV2_BUF',7:'CV_REF',8:'+3V2',9:'UI_SDA',10:'UI_SCL'},7,49,20.5,0,'C37593',[(str(n),nm,typ) for n,nm,typ in [(1,'ADDR','input'),(2,'ALERT_RDY','open_collector'),(3,'GND','power_in'),(4,'AIN0','input'),(5,'AIN1','input'),(6,'AIN2','input'),(7,'AIN3','input'),(8,'VDD','power_in'),(9,'SDA','bidirectional'),(10,'SCL','input')]])
    res('R66','10k','+3V2','CV_BIAS_DIV',24,35)
    res('R67','10k','CV_BIAS_DIV','GND',26.5,35)
    cap('C53','CV_BIAS_DIV','GND',21,35)
    for k,y in [(1,24),(2,33)]:
        x=15 if k==1 else 57
        res(f'R{66+2*k}','100k 0.1%',f'CV{k}_IN',f'CV{k}_NODE',x,y,'C855559')
        res(f'R{67+2*k}','10k 0.1%',f'CV{k}_NODE','CV_BIAS',x,y+2.5,'C95204')
        cap(f'C{53+k}',f'CV{k}_NODE','GND',x,y+5)
        add(f'D{2+k}','BAT54S','Package_TO_SOT_SMD:SOT-23',{1:'GND',2:'+3V2',3:f'CV{k}_NODE'},7,x+3.5,y+2.5,0,'C83935',[('1','LOW_ANODE','passive'),('2','HIGH_CATHODE','passive'),('3','COMMON','passive')])
    res('R72','100k 0.1%','GND','CV_REF_NODE',25,44,'C855559');res('R73','10k 0.1%','CV_REF_NODE','CV_BIAS',25,41.5,'C95204')
    cap('C56','CV_REF_NODE','GND',22,43);cap('C57','+3V2','GND',17,38);cap('C58','+3V2','GND',46,20.5)
    res('R74','2.2k','+3V2','GND',51,17,'C4190')
    for k in [1,2]:
        x=15 if k==1 else 66; y=31
        add(f'Q{2+k}','MMBT3904-7-F','Package_TO_SOT_SMD:SOT-23',{1:f'GATE{k}_BASE',2:'GND',3:f'GATE{k}_N'},7,x,y,0,'C94514',[('1','B','input'),('2','E','passive'),('3','C','open_collector')])
        res(f'R{73+3*k}','100k',f'GATE{k}_IN',f'GATE{k}_BASE',x,y-3,'C25803')
        res(f'R{74+3*k}','100k',f'GATE{k}_BASE','GND',x+3,y-3,'C25803')
        res(f'R{75+3*k}','10k',f'GATE{k}_N','+3V2',x+3,y,'C25804')
        add(f'D{4+k}','1N4148W','Diode_SMD:D_SOD-123',{1:f'GATE{k}_BASE',2:'GND'},7,x,y+3,0,'C81598',[('1','K','passive'),('2','A','passive')])
    for ref,net,x,y,rot in [('J6','CV1_IN',8.3,19.5,0),('J7','GATE1_IN',8.3,31,0),('J8','CV2_IN',76.7,19.5,180),('J9','GATE2_IN',76.7,31,180)]:
        add(ref,net,'Chiptune:PJ320D4A',{1:'GND',2:net,3:'GND',4:None},7,x,y,rot,'C95562',[('1','SLEEVE','passive'),('2','TIP','passive'),('3','RING_GND','passive'),('4','NC','passive')])
    add('J5','HS96L03W2C03 OLED INTERFACE','Chiptune:OLED_HS96_INTERFACE',{1:'GND',2:'+3V2',3:'UI_SCL',4:'UI_SDA'},8,31,32.5,0,'C5248080',[('1','GND','power_in'),('2','VCC','power_in'),('3','SCL','input'),('4','SDA','bidirectional')],notes='Manual assembly; 27.3x27.8mm SSD1315 module on 5mm M2 spacers, not SMT placed')
    for k,(ref,net,x) in enumerate([('SW5','UI_MINUS',48),('SW6','UI_OK',57),('SW7','UI_PLUS',66)]):
        add(ref,net,'Chiptune:TS1187A',{1:net,2:'GND'},8,x,50,0,'C318884',[('1','1','passive'),('2','2','passive')]);res(f'R{82+k}','10k',net,'+3V2',x,46,'C25804',8)
    for ref,net,x in [('JP1','BOOT_GND',10),('JP2','RUN',15)]:
        add(ref,'BOOT' if ref=='JP1' else 'RESET','Chiptune:SERVICE_PADS',{1:net,2:'GND'},1,x,48,0,'',[('1','1','passive'),('2','2','passive')],notes='Rear service pads; short briefly for BOOT/RESET')
    # KiCad recognises _P/_N differential-pair suffixes.
    usb_names={'USB_DP':'USB_D_P','USB_DM':'USB_D_N','USB_DP_MCU':'USB_MCU_D_P','USB_DM_MCU':'USB_MCU_D_N'}
    for o in parts:o['nets']={n:usb_names.get(v,v) for n,v in o['nets'].items()}
    # Identical 15 pF capacitors exchange nodes to avoid crossing crystal nets.
    by['C1']['nets'][1]='XTAL_OUT';by['C2']['nets'][1]='XIN'
    assert len(parts)==len({o['ref'] for o in parts})
