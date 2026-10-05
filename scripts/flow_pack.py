"""Pad-aware packing: local feedback/input nodes precede long low-impedance nets."""
import math

def pack(b,fps,targets,bbox,overlaps,v,p,freeze=False):
    anchors={'U1','U2','Y1','U3','L1','U5','U9','U10','U11','U12','U13','U14','U15','U16','J1','J2','J3','J4','J5','J6','J7','J8','J9','H1','H2','H3','H4','SW5','SW6','SW7','JP1','JP2', *[f'C{i}' for i in range(1,16)],*[f'C{i}' for i in range(28,34)], 'C16','C18','C19','C21','C22','C34','C36','C57','C58','R1','R2','R4','R5','R42','R43','R44','R38','D1','D5','C43','C45','R49','C42','R52','C44','D2','R55','R11','R12'}
    anchors.update(['Q4','D6','R79','R80','R81'])
    anchors.update(['Q3','R76','R77','R78'])
    anchors.update(['R6','R7','R8'])
    anchors.add('R39')
    anchors.update(['R26','R27'])
    if freeze:anchors=set(fps)
    fixed={r:bbox(f) for r,f in fps.items() if r in anchors and r not in ['J5','JP1','JP2']}
    obstacles={**fixed};holeboxes=[]
    for r,f in fps.items():
        for d in f.Pads():
            if d.GetAttribute()==p.PAD_ATTRIB_NPTH:
                x=p.ToMM(d.GetPosition().x);y=p.ToMM(d.GetPosition().y);rad=p.ToMM(d.GetSize().x)/2+.8
                holeboxes.append((x-rad,y-rad,x+rad,y+rad))
    cx=p.ToMM(fps['J5'].GetPosition().x);cy=p.ToMM(fps['J5'].GetPosition().y)
    holeboxes.append((cx-4.8,cy-13.6,cx+4.8,cy-11.5))
    conflicts=[]
    for i,(r,a) in enumerate(fixed.items()):
        for z,bb in list(fixed.items())[:i]:
            if overlaps(a,bb):conflicts.append((r,z))
        if not r.startswith(('J','H')):
            if any(overlaps(a,bb) for bb in holeboxes):conflicts.append((r,'mount/header'))
    if conflicts:raise ValueError('Fixed conflicts '+str(conflicts))
    if freeze:return {}
    # Explicit regions keep the analog and switching blocks separate.
    regions={}
    def region(refs,box):
        for r in refs:regions[r]=box
    region(['R3'],(25,33,44,43))
    region(['R26','R27'],(22,14,38,38))
    region(['R39'],(27,38,37,47))
    region(['R11','R12'],(37.5,31.5,42.5,39.5))
    region(['R6','R7','R8'],(26,18,35,26))
    region(['R16','R17'],(38,20,49,23.5))
    region(['R28','R29','R30','R31','R32','R35','R36','R37'],(16,14.5,45,24))
    region(['R38','D1'],(15.5,32,28,35))
    region(['R40','R41','C37','C35','FB1'],(51,30,62,41))
    region(['R45','R46','R49','C38','C39','C42'],(51,30,64,48))
    region(['R47','R48','R52','C40','C41','C44'],(60.5,31,72,46))
    region(['C43','C45','R50','R51','R53','R54'],(55,42.5,79,51.5))
    region(['R58','R59','R61','R62','R63','R64','C50','C51','C52'],(23,15,45,51))
    region(['R66','R67','C53'],(48,18,53.5,23))
    region(['R68','R69','C54','D3'],(49,22,55,33))
    region(['R70','R71','C55','D4'],(55,27.5,65,34))
    region(['R72','R73','C56'],(56,16,65,21))
    region(['Q3','D5','R76','R77','R78'],(15.5,22,22,33.5))
    region(['Q4','D6','R79','R80','R81'],(62,26.5,69.5,33.5))
    region(['R82','R83','R84'],(25,38,44,44))
    weights={'XIN':15,'XOUT':15,'XTAL_OUT':15,'FLASH_CS':2,'USB_DP':4,'USB_DM':4,'USB_DP_MCU':6,'USB_DM_MCU':6,'BOOST_FB':15,'CV_BIAS_DIV':8,'CV1_IN':.12,'CV2_IN':.12,'CV1_NODE':12,'CV2_NODE':12,'CV_REF_NODE':12,'CV_BIAS':3,'CV1_BUF':4,'CV2_BUF':4,'CV_REF':4,'SUM_L':12,'SUM_R':12,'MIX_L':2,'MIX_R':2,'VREF_DIV':8,'VREF':2,'MIDI_ANODE':4,'MIDI_TIP':2,'MIDI_RING':1,'MIDI_RX':3,'GATE1_BASE':6,'GATE2_BASE':6,'CC1_ADC':6,'CC2_ADC':6,'CC1':.5,'CC2':.5}
    movable=[r for r in fps if r not in anchors]
    caches={}
    positions={}
    def readpads(r):return [(d.GetNumber(),d.GetNetname(),p.ToMM(d.GetPosition().x),p.ToMM(d.GetPosition().y)) for d in fps[r].Pads() if d.GetNetname()]
    for r in fps:positions[r]=readpads(r)
    for r in movable:
        f=fps[r];x=p.ToMM(f.GetPosition().x);y=p.ToMM(f.GetPosition().y);angles=[0,90,180,270]
        caches[r]={}
        for angle in angles:
            f.SetOrientationDegrees(angle);a=bbox(f)
            caches[r][angle]=((a[0]-x,a[1]-y,a[2]-x,a[3]-y),[(n,net,px-x,py-y) for n,net,px,py in readpads(r)])
    # Pick larger protection parts first, then paired resistors/caps.
    critical={'Q3','D5','R76','R77','R78','Q4','D6','R79','R80','R81','R66','R67','C53','R68','R69','C54','D3','R70','R71','C55','D4','R72','R73','C56','R6','R7','R8'}
    critical.update(['R45','R46','C38','C39','R47','R48','C40','C41','R40','R41','C37','C35','FB1','R50','R51','R53','R54'])
    def priority(r):
        if r in ['R66','R67','C53','R68','R69','C54','D3','R70','R71','C55','D4','R72','R73','C56']:return 0
        if r in ['Q3','R76','R77','R78','Q4','D6','R79','R80','R81','R6','R7','R8','R26','R27','R3','R16','R17']:return 1
        if r in critical:return 2
        return 3
    movable.sort(key=lambda r:(priority(r),-(caches[r][0][0][2]-caches[r][0][0][0])*(caches[r][0][0][3]-caches[r][0][0][1])))
    results={}
    for sweep in range(4):
        order=movable if sweep%2==0 else movable[::-1]
        for r in order:
            others=[a for z,a in obstacles.items() if z!=r]+holeboxes
            netothers={}
            for z,pads in positions.items():
                if z==r:continue
                for n,net,x,y in pads:netothers.setdefault(net,[]).append((x,y,z))
            tx,ty=targets[r][:2] if r in targets else (p.ToMM(fps[r].GetPosition().x)-100,p.ToMM(fps[r].GetPosition().y)-100)
            box=regions.get(r,(max(.8,tx-8),max(.8,ty-8),min(84.2,tx+8),min(53.2,ty+8)))
            best=None
            for angle,(local,pads) in caches[r].items():
                for ix in range(round(box[0]*2),round(box[2]*2)+1):
                    x=100+ix*.5
                    for iy in range(round(box[1]*2),round(box[3]*2)+1):
                        y=100+iy*.5;a=(x+local[0],y+local[1],x+local[2],y+local[3])
                        if a[0]<100.6 or a[1]<100.6 or a[2]>184.4 or a[3]>153.4:continue
                        if any(overlaps(a,bb) for bb in others):continue
                        score=.12*(abs(x-100-tx)+abs(y-100-ty))
                        for n,net,dx,dy in pads:
                            if net=='GND' or net.startswith('+') or net in ['USB_VBUS']:continue
                            mates=netothers.get(net,[])
                            if mates:
                                w=weights.get(net,1)
                                # Higher impedance nodes strongly cluster around their active pin.
                                ds=[abs(x+dx-xx)+abs(y+dy-yy) for xx,yy,z in mates]
                                score+=w*sum(ds)/len(ds)
                        candidate=(score,abs((angle-(targets.get(r,[0,0])+[0])[2])%360),ix,iy,angle,a,pads)
                        if best is None or candidate[:5]<best[:5]:best=candidate
            if best is None:
                import json
                from pathlib import Path
                Path('outputs/flow-pack-debug.json').write_text(json.dumps({'failed':r,'boxes':obstacles,'placed':results},indent=2))
                raise ValueError('No local placement for '+r+' in '+str(box))
            score,_,ix,iy,angle,a,pads=best;x=100+ix*.5;y=100+iy*.5
            fps[r].SetOrientationDegrees(angle);fps[r].SetPosition(v(x,y));positions[r]=[(n,net,x+dx,y+dy) for n,net,dx,dy in pads]
            obstacles[r]=a;results[r]=[ix*.5,iy*.5,angle]
        print('Flow placement sweep',sweep+1,flush=True)
    return results
