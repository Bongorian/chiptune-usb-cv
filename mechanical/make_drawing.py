#!/usr/bin/env python3
"""Dimensioned reference drawing; annotations do not appear on printed parts."""
import json
from pathlib import Path
import cairosvg
HERE=Path(__file__).resolve().parent
B=json.loads((HERE/'board_features.json').read_text())
P=json.loads((HERE/'parameters.json').read_text())
M=json.loads((HERE/'assembly_metadata.json').read_text())
OUT=HERE/'previews'
elements=[]

def line(x1,y1,x2,y2,color='#4a5f65',dash=False):
 elements.append(f'<path d="M{x1},{y1} L{x2},{y2}" fill="none" stroke="{color}" stroke-width="1.4"'+(' stroke-dasharray="5 4"' if dash else '')+'/>')
def rect(x,y,w,h,r=0,fill='none',stroke='#334e56'):
 elements.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="1.4"/>')
def text(x,y,t,size=13,fill='#30484f',anchor='start'):
 elements.append(f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-family="DejaVu Sans, sans-serif">{t}</text>')
def circle(x,y,r,fill='none',stroke='#334e56'):
 elements.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="1.4"/>')
text(50,42,'CHIPTUNE USB/CV  /  ENCLOSURE',23)
text(50,64,'Dimensions in mm  /  diagram not to scale  /  drawing-only annotations',12)
# Top projection, board coordinate Y inverted for the drawing.
s=5.0; ox=110; oy=132
pt=lambda x,y:(ox+x*s,oy+(54-y)*s)
x,y=pt(-2.4,56.4)
rect(x,y,89.8*s,58.8*s,5.4*s,fill='#eff4f4')
x,y=pt(0,54);rect(x,y,85*s,54*s,3*s,stroke='#bdc9cc')
for v in M['windows']:
 x,y=pt(v['x']-v['w']/2,v['y']+v['d']/2)
 rect(x,y,v['w']*s,v['d']*s,v['r']*s,fill='#c9e3e8')
for ref in ['SW5','SW6','SW7']:
 x,y=pt(*B['features'][ref]['center']);circle(x,y,2.95*s,fill='#d3dcde')
for ref in ['H1','H2','H3','H4']:
 x,y=pt(*B['features'][ref]['center']);circle(x,y,1.1*s,stroke='#6e999d')
for ref,label in [('J6','CV1'),('J7','GATE1'),('J2','MIDI IN')]:
 _,yp=B['features'][ref]['center'];x,y=pt(-2.4,yp)
 line(x-16,y,x+4,y);text(x-20,y+4,label,11,anchor='end')
for ref,label in [('J8','CV2'),('J9','GATE2'),('J3','LINE OUT')]:
 _,yp=B['features'][ref]['center'];x,y=pt(87.4,yp)
 line(x-4,y,x+12,y);text(x+18,y+4,label,11)
x,y=pt(34,-2.4);line(x,y,x,y+15);text(x,y+31,'USB-C',11,anchor='middle')
x,y=pt(74,-2.4);line(x,y,x,y+15);text(x,y+31,'LED slot',11,anchor='middle')
x,y=pt(57,4);text(x,y-23,'- / OK / +',11,anchor='middle')
x,y=pt(42.5,45.5);text(x,y+4,'SAA1099  /  SN76489AN x2',11,anchor='middle')
x,y=pt(42.5,M['oled_active_y']);text(x,y+4,'OLED',12,anchor='middle')
# Overall dimensions.
x1,y1=pt(-2.4,56.4);x2,y2=pt(87.4,56.4)
line(x1,y1-24,x2,y2-24)
for x in [x1,x2]:line(x,y1-29,x,y1+2)
text((x1+x2)/2,y1-31,'89.8',14,anchor='middle')
x1,y1=pt(-2.4,56.4);x2,y2=pt(-2.4,-2.4)
line(x1-39,y1,x2-39,y2)
for y in [y1,y2]:line(x1-44,y,x1+2,y)
text(x1-43,(y1+y2)/2,'58.8',13,anchor='end')
text(82,492,'TOP  /  PCB hole centres: (3,3), (82,3), (3,51), (82,51)',12)
text(82,514,'Four M2 x 10 screws from the underside; heat-set inserts in lid posts.',12)
text(82,535,'Blue circles: PCB holes, hidden under the assembled lid.',11)
# Right-side view: circles allow the plug shoulder into the recessed sockets.
s2=4.7; x0=671;y0=257
rect(x0,y0-23*s2,58.8*s2,23*s2,fill='#eff4f4')
line(x0,y0-M['seam_z']*s2,x0+58.8*s2,y0-M['seam_z']*s2,dash=True)
for board_y in [11.5,23,34.5]:
 cx=x0+(board_y+2.4)*s2;cy=y0-M['seam_z']*s2
 circle(cx,cy,4.75*s2,fill='white')
line(x0+58.8*s2+13,y0-23*s2,x0+58.8*s2+13,y0)
text(x0+58.8*s2+19,y0-10*s2,'23',13)
text(x0,291,'SIDE  /  TRS relief: diameter 9.5',12)
text(x0,312,'Port pitch: 11.5  /  seam at jack axis',12)
# Height stack section, drawn separately from the external profile.
text(671,358,'HEIGHT STACK',15)
ybase=483;xs=725
rect(xs,ybase-2*s2,160,2*s2,fill='#93aab0')
rect(xs,ybase-7.6*s2,160,1.6*s2,fill='#629b7d')
rect(xs+26,ybase-12.6*s2,13,5*s2,fill='#b8c8cc')
rect(xs+120,ybase-12.6*s2,13,5*s2,fill='#b8c8cc')
rect(xs+23,ybase-13.8*s2,113,1.2*s2,fill='#629b7d')
rect(xs+26,ybase-15.7*s2,106,1.9*s2,fill='#344751')
rect(xs+13,ybase-21*s2,15,2*s2,fill='#93aab0')
rect(xs+134,ybase-21*s2,15,2*s2,fill='#93aab0')
line(xs-15,ybase-6*s2,xs+175,ybase-6*s2,dash=True)
for z,label in [(6,'PCB bottom: 6.0'),(12.6,'OLED bottom: 12.6'),(21,'Open bezel: 19.0 - 21.0')]:
 text(xs+179,ybase-z*s2+4,label,10)
text(671,512,'Separate OLED module on 5 mm PCB spacers',12)
# Fit coupon coordinates (x/y), order unambiguous with no engraved labels.
text(82,570,'FIT COUPON  /  58 x 16 x 5',16)
s3=4.5;xc=82;yc=675
rect(xc,yc-16*s3,58*s3,16*s3,2*s3,fill='#eff4f4')
for i,dia in enumerate([2.9,3.0,3.1,3.2,3.3]):
 xx=xc+(5+10*i)*s3;yy=yc-5*s3;circle(xx,yy,dia*s3/2)
 text(xx,yc+19,str(dia),11,anchor='middle')
for i,dia in enumerate([5.7,5.9,6.1]):
 xx=xc+(15+14*i)*s3;yy=yc-11*s3;circle(xx,yy,dia*s3/2)
 text(xx,yc-16*s3-9,str(dia),11,anchor='middle')
text(377,623,'Blind holes: insert pilot diameters',12)
text(377,646,'Through holes: button guide diameters',12)
text(671,570,'PRINT PARTS',16)
for j,t in enumerate(['Base x1  /  bottom on bed','Lid x1  /  exterior face on bed',
                       'Button cap x3  /  head face on bed','Open IC / OLED apertures; no covers',
                       'Case M2 inserts: OD 3.5, length 3.0']):text(671,599+23*j,t,12)
text(50,753,'Prototype fit required: insert bore, switch travel, socket height and actual module dimensions.',11)
svg='<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="790" viewBox="0 0 1100 790"><rect width="1100" height="790" fill="white"/>'+''.join(elements)+'</svg>'
(OUT/'mechanical_drawing.svg').write_text(svg)
cairosvg.svg2png(bytestring=svg.encode(),write_to=str(OUT/'mechanical_drawing.png'),scale=1.5)
cairosvg.svg2pdf(bytestring=svg.encode(),write_to=str(OUT/'mechanical_drawing.pdf'))
print('Mechanical drawing generated')
