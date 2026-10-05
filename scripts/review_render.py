#!/usr/bin/python3
"""Dimension-based placement review diagrams drawn from the actual KiCad board."""
import pcbnew as p,math,json,html
from pathlib import Path
b=p.LoadBoard('hardware/Chiptune_USB_CV.kicad_pcb')
fps={f.GetReference():f for f in b.GetFootprints()}
def mm(n):return p.ToMM(n)
def esc(t):return html.escape(str(t))
def rect(x,y,w,h,fill,stroke='#989c9a',sw=.08,rad=.1):return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rad}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>'
def text(t,x,y,size=.8,color='#fff',anchor='middle',weight=400):return f'<text x="{x}" y="{y}" fill="{color}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" font-family="Noto Sans CJK JP,DejaVu Sans,sans-serif">{esc(t)}</text>'
body_dims={'U1':(7,7),'U2':(5.3,5.3),'U3':(1.6,1.2),'U4':(3,1.3),'U5':(3,1.5),'U6':(1.6,1.6),'U7':(3,1.75),'U8':(2,1.25),'U9':(3,1.75),'U13':(6.4,9.4),'U14':(4.4,5),'U15':(4.4,5),'U16':(3,3),'Q1':(3,1.3),'Q2':(3,1.3),'Y1':(3.2,2.5),'D1':(1.8,1.2),'D2':(1.6,.8),'L1':(5.4,5.1),'SW1':(6.7,2.6)}
labels={'U1':'RP2040','U2':'FLASH','U10':'SAA1099','U11':'SN76489AN 1','U12':'SN76489AN 2','U13':'6N137','U14':'MIXER','Y1':'12 MHz','L1':'1 µH','U5':'3.2V','U15':'CV BUF','U16':'ADC','U3':'BOOST'}
body_dims.update({'Q3':(3,1.4),'Q4':(3,1.4),'D3':(3,1.4),'D4':(3,1.4),'D5':(1.8,1.2),'D6':(1.8,1.2)})

def drawing(zoom=False):
 s=rect(0,0,85,54,'#153d34','#91ad9c',.1,3)
 body_svg=[]; holes_svg=[]
 # Paint package bodies in local coordinates; placement is taken from KiCad.
 for ref,f in fps.items():
  if f.GetLayer()!=p.F_Cu or ref.startswith('H'):continue
  x=mm(f.GetPosition().x)-100;y=mm(f.GetPosition().y)-100;ang=-f.GetOrientationDegrees()
  part=''
  if ref in ['U10','U11','U12']:
   h=22.98 if ref=='U10' else 20.44
   part=rect(-1.27,-1.33,10.16,h,'#161c20','#666d70',.1,.45)
   part+=f'<path d="M 2.9 -1.25 Q 3.81 .6 4.72 -1.25" fill="none" stroke="#8a9397" stroke-width=".25"/>'
  elif ref in ['J2','J3','J6','J7','J8','J9']:
   part=rect(-6.35,-3.05,12.10,6.10,'#171c20','#747c7d',.1,.15)+rect(-8.35,-2.5,2,5,'#59625f','#ced9d3',.1,.1)
  elif ref=='J1':
   part=rect(-4.47,-3.65,8.94,7.3,'#adb8b3','#d6dfd8',.12,.3)+rect(-3.65,2.6,7.3,1.05,'#293c36','#e2eee5',.08,.05)
  elif ref=='C26':
   part='<circle cx="0" cy="0" r="3.15" fill="#b9bebb" stroke="#dcdfdd" stroke-width=".13"/><path d="M -2.3 -2.2 A 3.15 3.15 0 0 0 -2.3 2.2 Z" fill="#3b4341"/>'+text('100 µF',.4,.25,.8,'#202723')
  elif ref.startswith('SW') and ref!='SW1':
   part=rect(-2.55,-2.55,5.1,5.1,'#bac5c0','#e4ebe6',.1,.25)+'<circle cx="0" cy="0" r="1.05" fill="#34433d" stroke="#75827d" stroke-width=".1"/>'
  elif ref=='J4':
   part=rect(-1.27,-1.27,2.54,10.16,'#192421','#6a7e70',.1,.1)
  elif ref=='SW1':
   part=rect(-3.35,-1.4,6.7,2.8,'#303c38','#bccbc1',.1,.15)+rect(.15,1.4,1.3,1.45,'#1b2421','#bccbc1',.1,.1)
  else:
   if ref in body_dims:w,h=body_dims[ref]
   elif ref.startswith(('R','FB')):w,h=1.6,.8
   elif ref.startswith('C'):w,h=(2,1.25) if ref in ['C18','C19','C35'] else(1.6,.8)
   else:continue
   fill='#8d6b3f' if ref.startswith('C') else('#42484b' if ref=='L1' else '#20262a')
   if ref=='D2':fill='#85e7a0'
   part=rect(-w/2,-h/2,w,h,fill,'#82968a',.055,.07)
   if ref.startswith(('U','Q')):part+='<circle cx="'+str(-w/2+.28)+'" cy="'+str(-h/2+.28)+'" r=".10" fill="#bdc9c2"/>'
  if ref in ['U10','U11','U12','J4']:s+=f'<g transform="translate({x},{y}) rotate({ang})">{part}</g>'
  else:body_svg.append(f'<g transform="translate({x},{y}) rotate({ang})">{part}</g>')
 # Show the exposed lands; copper is unconnected in this review board.
 for ref,f in fps.items():
  for pad in f.Pads():
   x=mm(pad.GetPosition().x)-100;y=mm(pad.GetPosition().y)-100
   if pad.GetAttribute()==p.PAD_ATTRIB_NPTH:
    holes_svg.append(f'<circle cx="{x}" cy="{y}" r="{mm(pad.GetDrillSize().x)/2}" fill="#f5f6f3" stroke="#8da397" stroke-width=".12"/>')
   elif f.GetLayer()==p.F_Cu:
    w=mm(pad.GetSize().x);h=mm(pad.GetSize().y);a=-pad.GetOrientationDegrees()
    if ref.startswith('U') and ref not in ['U10','U11','U12'] and w>=1.5 and h>=1.5:continue # Exposed pad beneath package.
    fill='#d5c79b' if ref in ['U10','U11','U12','J4'] else'#d7dfda'
    s+=f'<g transform="translate({x},{y}) rotate({a})">'+rect(-w/2,-h/2,w,h,fill,'#7c9185',.04,.07)+'</g>'
    if pad.GetAttribute()==p.PAD_ATTRIB_PTH:
     s+=f'<circle cx="{x}" cy="{y}" r="{mm(pad.GetDrillSize().x)/2}" fill="'+('#111715' if ref in ['U10','U11','U12','J4'] else'#f5f6f3')+'"/>'
 s+=''.join(body_svg)+''.join(holes_svg)
 # Main package labels, orthogonal and aligned with their package.
 for ref,t in labels.items():
  f=fps[ref];x=mm(f.GetPosition().x)-100;y=mm(f.GetPosition().y)-100
  if ref in ['U10','U11','U12']:
   angle=-f.GetOrientationDegrees();a=math.radians(angle);ly=10.16 if ref=='U10' else 8.89
   xx=x+math.cos(a)*3.81-math.sin(a)*ly; yy=y+math.sin(a)*3.81+math.cos(a)*ly
   s+=f'<g transform="translate({xx},{yy}) rotate({angle+90})">'+text(t,0,.35,1.15)+'</g>'
  else:
   size=1.15 if ref=='U1' else(.75 if ref in ['U5','U3'] else.85)
   s+=text(t,x,y+.28,size)
 if not zoom:
  for t,x,y in [('CV 1',7,14.7),('GATE 1',7,25.7),('MIDI A',7,37.4),('CV 2',78,14.7),('GATE 2',78,25.7),('LINE OUT',78,37.4),('USB-C',34,53.2),('SWD',11.8,47.6),('−',48,53.8),('OK',57,53.8),('+',66,53.8)]:s+=text(t,x,y,.85,'#d8e8dc')
  s+=text('CHIPTUNE',62,17,1.3,'#d8e8dc',weight=500)
  for r in ['U10','U11','U12']:
   x=mm(fps[r].GetPosition().x)-100
   s+=text('100n / 4.7µ',x+1.75,.5,.65,'#d8e8dc')
 else:
  for ref,f in fps.items():
   x=mm(f.GetPosition().x)-100;y=mm(f.GetPosition().y)-100
   if f.GetLayer()!=p.F_Cu or not(19<x<51 and 20<y<46):continue
   if ref.startswith(('C','R')) and ref!='C26':
    angle=-f.GetOrientationDegrees()
    s+=f'<g transform="translate({x},{y}) rotate({angle})">'+text(ref,0,.16,.5,'#182b22' if ref.startswith('C') else '#edf4ee')+'</g>'
 return s


base=drawing()
def module():
 s=rect(17.35,18.6,27.3,27.8,'#1d2530','#617384',.12,.5)
 s+=rect(18.85,24,24.3,17.7,'#10141a','#687678',.1,.1)
 s+=rect(20.13,25.6,21.74,10.86,'#050708','#b0bcb7',.05,0)
 for t,y in [('MIX / PRESET 01',27.25),('SAA  75%  L+R',29.1),('SN1  60%  LEFT',30.95),('SN2  60%  RIGHT',32.8),('CV1: LEAD',34.65)]:s+=text(t,21,y,1.05,'#f1f8ee','start')
 for x in [19.35,42.65]:
  for y in [20.6,44.4]:s+=f'<circle cx="{x}" cy="{y}" r="1.55" fill="#b5bdc7" stroke="#82909a" stroke-width=".1"/><path d="M {x-.65} {y} H {x+.65} M {x} {y-.65} V {y+.65}" stroke="#273540" stroke-width=".18"/>'
 for i,x in enumerate([27.19,29.73,32.27,34.81]):
  s+=rect(x-.55,19.37,1.1,1.1,'#a7afad','#566b60',.08,.05)
 s+=text('0.96″ / SSD1315',31,40.6,.85,'#cedcde')
 dx=mm(fps['J5'].GetPosition().x)-100-31;dy=mm(fps['J5'].GetPosition().y)-100-32.5
 return f'<g transform="translate({dx},{dy})">'+s+'</g>'

def page(assembled):
 s='<svg xmlns="http://www.w3.org/2000/svg" viewBox="-5 -9 100 82" width="1600" height="1312">'+rect(-5,-9,100,82,'#f5f6f3','#f5f6f3',0,0)
 s+=text('USB / CV 配置案・配線前'+('' if assembled else ' / 表示器を外した状態'),0,-5,2,'#233f34','start',500)
 s+=text('85 × 54 mm・角 R3・M2穴は角R中心・電池なし',0,-1.7,1.2,'#576a60','start')+base
 if assembled:s+=module()
 s+='<g fill="none" stroke="#62766b" stroke-width=".12"><path d="M 0 56 V 60 M 85 56 V 60 M 0 58 H 85 M 88 0 H 92 M 88 54 H 92 M 90 0 V 54"/></g>'
 s+=text('85 mm',42.5,57.7,1.25,'#40574a')+'<g transform="translate(91,27) rotate(-90)">'+text('54 mm',0,0,1.25,'#40574a')+'</g>'
 s+=text('左：CV1・GATE1・MIDI ／ 右：CV2・GATE2・LINE OUT',42.5,63,1.2,'#40574a')
 s+=text('表示器は5 mmスペーサーで浮かせます。下のSMT部品との重なりは意図した構造です。',42.5,66,1.1,'#40574a')
 s+=text('実寸フットプリントから作図。画面表示は模式図。',42.5,69,1.05,'#40574a')+'</svg>'
 return s
Path('outputs/review/placement-front.svg').write_text(page(True))
Path('outputs/review/placement-display-removed.svg').write_text(page(False))
Path('outputs/review/rp2040-closeup.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="19 20 32 26" width="1600" height="1300">'+drawing(True)+'</svg>')
# Section: PCB surfaces, not a perspective image; spacers provide 5mm to OLED underside.
s='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 90 30" width="1600" height="534">'+rect(0,0,90,30,'#f5f6f3','#f5f6f3',0,0)
s+=text('表示器の取り付け・断面案',3,4,2,'#233f34','start',500)
s+=rect(7,22,72,1.6,'#153d34','#527765',.1,0)+text('メイン基板',80,23.3,1.2,'#40574a','start')
s+=rect(19,15.8,35,1.2,'#1d2530','#617384',.1,0)+text('OLED基板',55,16.7,1.2,'#40574a','start')
for x in [21,50]:s+=rect(x,17,2,5,'#adb5b5','#7c8883',.1,0)
s+=rect(33,20.5,7,1.5,'#20262a','#7c8883',.1,0)+text('RP2040 / SMT',36.5,26.5,1.2,'#40574a')
s+=rect(26,17,3,1,'#20262a','#7c8883',.1,0)
s+='<path d="M 12 17 H 17 M 12 22 H 17 M 14 17 V 22" fill="none" stroke="#40574a" stroke-width=".12"/>'+text('5 mm',10,20.7,1.2,'#40574a','end')
# Not drawn at the same vertical scale: dimensional note controls assembly.
s+=text('スペーサー長 5 mm。基板下面の部品・端子も確認して組み立て。',45,29,1.1,'#40574a')+'</svg>'
Path('outputs/review/oled-section.svg').write_text(s)
print('Generated USB/CV assembly, exposed-board and MCU placement views')
