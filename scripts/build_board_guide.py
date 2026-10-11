#!/usr/bin/env python3
"""Create the Japanese board guide from the final design, without external services.
Requires reportlab and an embeddable Japanese font (Droid Sans Fallback on Ubuntu).
All page graphics are explanatory drawings or existing native CAD renders.
"""
from pathlib import Path
import hashlib, json, re, math
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor, Color, white
from reportlab.lib.pagesizes import A4
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output/pdf'; OUT.mkdir(parents=True,exist_ok=True)
PDF=OUT/'Chiptune_USB_CV_Guide_JA.pdf'
JP_FONT=Path('/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf')
if not JP_FONT.exists(): raise RuntimeError('Set JP_FONT to an embeddable Japanese TrueType font.')
pdfmetrics.registerFont(TTFont('JP',str(JP_FONT)))
pdfmetrics.registerFont(TTFont('Latin','/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('LatinB','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'))
PAGE_W,PAGE_H=A4
INK=HexColor('#18363d'); TEAL=HexColor('#127c7f'); MUTED=HexColor('#556c72')
LIGHT=HexColor('#edf4f4'); LINE=HexColor('#d4e0e2'); ORANGE=HexColor('#b96130')
X=42; CW=PAGE_W-2*X
C=canvas.Canvas(str(PDF),pagesize=A4,pageCompression=1)
C.setTitle('Chiptune USB/CV - 基板説明資料'); C.setAuthor('Chiptune USB/CV Project')
C.setSubject('ハードウェア構成・端子・信号経路・操作設計・組み立て・開発状況')
C.setCreator('ReportLab / scripts/build_board_guide.py')
page=0; bounds=[]
JP_CMAP=pdfmetrics.getFont('JP').face.charToGlyph

def runs(s):
 # Droid Sans Fallback intentionally omits many Latin glyphs; mix embedded fonts.
 out=[]
 for ch in s:
  font='JP' if ord(ch)>=0x300 and JP_CMAP.get(ord(ch),0) else 'Latin'
  if out and out[-1][0]==font:out[-1]=(font,out[-1][1]+ch)
  else:out.append((font,ch))
 return out

def mixed(s):
 out=[]
 for item in re.split(r'(<[^>]+>)',s):
  if item.startswith('<'):out.append(item)
  else:
   for font,chunk in runs(item):
    out.append('<font name="'+font+'">'+chunk+'</font>')
 return ''.join(out)


def rect(x,y,w,h,color=LIGHT,r=5,stroke=None):
 C.setFillColor(color);C.setStrokeColor(stroke or color)
 C.roundRect(x,PAGE_H-y-h,w,h,r,fill=1,stroke=bool(stroke))

def txt(s,x,y,size=10,col=INK,font='JP'):
 C.setFillColor(col)
 if font=='JP':
  for selected,chunk in runs(s):
   C.setFont(selected,size);C.drawString(x,PAGE_H-y-size,chunk)
   x+=pdfmetrics.stringWidth(chunk,selected,size)
 else:
  C.setFont(font,size);C.drawString(x,PAGE_H-y-size,s)

def para(s,x,y,w,size=10.3,col=INK,lead=None):
 st=ParagraphStyle('p',fontName='JP',fontSize=size,leading=lead or size*1.53,
                   textColor=col,wordWrap='CJK',spaceAfter=0)
 p=Paragraph(mixed(s),st); _,h=p.wrap(w,1000)
 assert y+h<773,(page,s[:40],y+h)
 p.drawOn(C,x,PAGE_H-y-h);bounds.append([page,round(y,2),round(y+h,2),s[:35]])
 return y+h

def line(x1,y1,x2,y2,col=LINE,width=1):
 C.setStrokeColor(col);C.setLineWidth(width);C.line(x1,PAGE_H-y1,x2,PAGE_H-y2)

def arrow(x1,y1,x2,y2,col=TEAL):
 line(x1,y1,x2,y2,col,1.4)
 a=math.atan2(y2-y1,x2-x1);l=5;w=2.6
 pts=[(x2,y2),(x2-l*math.cos(a)+w*math.sin(a),y2-l*math.sin(a)-w*math.cos(a)),
      (x2-l*math.cos(a)-w*math.sin(a),y2-l*math.sin(a)+w*math.cos(a))]
 p=C.beginPath();p.moveTo(pts[0][0],PAGE_H-pts[0][1])
 for x,y in pts[1:]:p.lineTo(x,PAGE_H-y)
 p.close();C.setFillColor(col);C.drawPath(p,fill=1,stroke=0)

def heading(n,title,subtitle):
 global page
 page+=1
 if page>1:C.showPage()
 rect(0,0,PAGE_W,7,TEAL,0)
 txt('CHIPTUNE USB/CV',X,23,9,TEAL,'LatinB')
 txt(f'{n:02d}',PAGE_W-X-20,21,12,TEAL,'LatinB')
 title_width=sum(pdfmetrics.stringWidth(chunk,font,21) for font,chunk in runs(title))
 txt(title,X,51,min(21,21*CW/title_width))
 para(subtitle,X,85,CW,9.2,MUTED)
 line(X,113,PAGE_W-X,113)
 line(X,787,PAGE_W-X,787)
 txt('Rev C hardware  |  2026-10-12  |  Design / prototype documentation',X,797,7,MUTED,'Latin')
 txt(f'{page} / 8',PAGE_W-X-27,797,8,MUTED,'Latin')
 C.bookmarkPage('p'+str(page));C.addOutlineEntry(title,'p'+str(page),0,False)

def section(title,y):
 txt(title,X,y,13,TEAL);return y+26

def note(s,y,height=49):
 rect(X,y,CW,height,LIGHT,5)
 para(s,X+12,y+9,CW-24,9.5)
 return y+height+12

def table(headers,rows,widths,y,size=9.2):
 xs=[X]
 for w in widths:xs.append(xs[-1]+w)
 def cell(s,x,yy,w,color=INK):
  st=ParagraphStyle('cell',fontName='JP',fontSize=size,leading=size*1.48,textColor=color,wordWrap='CJK')
  p=Paragraph(mixed(str(s)),st);_,h=p.wrap(w-14,1000);return p,h
 for i,row in enumerate([headers]+rows):
  vals=[cell(s,xs[j],y,widths[j],white if i==0 else INK) for j,s in enumerate(row)]
  h=max(v[1] for v in vals)+14
  assert y+h<773,(page,'table overflow',row)
  rect(X,y,sum(widths),h,TEAL if i==0 else (LIGHT if i%2 else white),0)
  for j,(p,ph) in enumerate(vals):p.drawOn(C,xs[j]+7,PAGE_H-y-7-ph)
  y+=h
  line(X,y,X+sum(widths),y,LINE,.5)
 return y+12

def image(path,x,y,w,h):
 im=ImageReader(str(path));iw,ih=im.getSize();sc=min(w/iw,h/ih)
 ww,hh=iw*sc,ih*sc
 C.drawImage(im,x+(w-ww)/2,PAGE_H-y-hh,ww,hh,mask='auto')
 return y+hh

def boxlabel(x,y,w,h,label,detail='',color=LIGHT):
 rect(x,y,w,h,color,6,LINE)
 para(label,x+9,y+8,w-18,10.5,TEAL)
 if detail:para(detail,x+9,y+27,w-18,8.5,MUTED)

def boardmap(x,y,w):
 # Board coordinate frame: X right; Y toward the sound IC row.
 scale=w/85;h=54*scale
 rect(x,y,w,h,HexColor('#1b6352'),3*scale)
 def pt(a,b):return x+a*scale,y+(54-b)*scale
 for a,b in [(3,3),(82,3),(82,51),(3,51)]:
  xx,yy=pt(a,b);C.setFillColor(white);C.circle(xx,PAGE_H-yy,1.1*scale,fill=1,stroke=0)
 for xx,label,ref,l in [(17.66,'SAA1099','U10',22.8),(42.89,'SN1','U11',20.6),(67.39,'SN2','U12',20.6)]:
  bx,by=pt(xx-l/2,50.4);rect(bx,by,l*scale,10.3*scale,HexColor('#092b2b'),2)
  txt(label,bx+10,by+11,9,white,'LatinB');txt(ref,bx+10,by+25,7,HexColor('#9bcabe'),'Latin')
  px,py=pt(xx-l/2+1.2,41.7);C.setFillColor(HexColor('#eeab60'));C.circle(px,PAGE_H-py,2,fill=1,stroke=0)
  # Notch at the left side of the IC package.
  line(bx+2,by+19,bx+7,by+19,HexColor('#a6cfc7'),2)
 # OLED footprint projected outline; separate module floats over the MCU.
 bx,by=pt(42.5-13.65,23+13.9)
 rect(bx,by,27.3*scale,27.8*scale,HexColor('#418875'),3,HexColor('#b6dbca'))
 txt('OLED',bx+36,by+25,12,white,'LatinB')
 txt('5 mm spacers',bx+14,by+44,8,white,'Latin')
 # Functional blocks visible outside the OLED projection.
 for a,b,label in [(20,30,'POWER'),(20.7,13,'MIDI'),(65,30,'CV / ADC'),(66,15,'MIXER')]:
  px,py=pt(a,b);txt(label,px-19,py-5,7.3,HexColor('#ddede5'),'LatinB')
 for a,b,ref,left in [(8.3,34.5,'J6',1),(8.3,23,'J7',1),(8.3,11.5,'J2',1),(76.7,34.5,'J8',0),(76.7,23,'J9',0),(76.7,11.5,'J3',0)]:
  px,py=pt(a,b);rect(px-6.0*scale,py-3.05*scale,12.1*scale,6.1*scale,HexColor('#142827'),2)
  txt(ref,px-5,py-4,7,white,'LatinB')
 px,py=pt(34,3.65);rect(px-4.47*scale,py-3.65*scale,8.94*scale,7.3*scale,HexColor('#d7e0e0'),1)
 txt('J1',px-6,py-5,7,INK,'LatinB')
 for a,ref in [(48,'SW5'),(57,'SW6'),(66,'SW7')]:
  px,py=pt(a,4);C.setFillColor(HexColor('#e5e6db'));C.circle(px,PAGE_H-py,2.55*scale,fill=1,stroke=0)
  txt(ref,px-7,py-3,5.5,INK,'Latin')
 return y+h

heading(1,'実物の音源ICで鳴らす、小型の音源基板','SAA1099 × 1 / SN76489AN × 2 / RP2040  |  ハードウェア概要・組み立て・開発資料')
y=para('Chiptune USB/CVは、3個のPSG音源ICをRP2040で制御する85 × 54 mmの音源基板です。USB MIDI、TRS MIDI、2組のCV／ゲート入力から演奏する構成を持ち、OLEDと3ボタンで音色・割当を設定する設計です。',X,133,CW,11)
y=image(ROOT/'mechanical/previews/assembled.png',X,y+16,CW,282)
para('ケース装着時のCADプレビュー。OLED表示は操作イメージで、実機画面ではありません。',X,y+5,CW,8.2,MUTED)
y=table(['項目','基板の仕様'],[
 ['音源','SAA1099 × 1、SN76489AN × 2。実ICのアナログ音声を合成'],
 ['入力','USB MIDIデバイス、TRS Type A MIDI IN、CV × 2、GATE × 2'],
 ['出力・操作','3.5 mmステレオライン出力、0.96インチOLED、3ボタン'],
 ['電源・寸法','USB-C給電、85 × 54 mm、4層FR-4、厚さ1.6 mm']], [93,CW-93],y+32,9.4)
note('開発状況：基板の配線・製造データ・設計検査は完了。ファームウェアv0.2.0-alphaはUF2ビルドとPCテストを通過。実基板での動作確認は未完了です。',y+3,61)

heading(2,'端子と部品配置','上から見て音源ICの列を奥、USB-Cとボタンを手前に置いた方向で説明します。')
y=boardmap(X+26,133,CW-52)
para('配置の説明図。部品外形は模式図です。オレンジの点は手実装する音源ICの1番ピン位置。',X,y+8,CW,8.2,MUTED)
y=table(['位置','参照番号','役割'],[
 ['左・奥','J6','CV1：1 V/octを想定した音程CV入力'],
 ['左・中央','J7','GATE1：CV1系統の発音／停止入力'],
 ['左・手前','J2','TRS MIDI IN：3.5 mm、Type A、絶縁受信'],
 ['右・奥','J8','CV2：2系統目の音程CV入力'],
 ['右・中央','J9','GATE2：CV2系統の発音／停止入力'],
 ['右・手前','J3','LINE OUT：3.5 mmステレオ、Tip=L / Ring=R / Sleeve=GND'],
 ['手前','J1','USB-C：給電とUSB MIDIデバイス通信'],
 ['手前右','SW5 / SW6 / SW7','− / 決定 / ＋。D2は状態表示用の赤LED']], [75,97,CW-172],y+30,8.7)
para('USB MIDI鍵盤を直接つなぐホスト機能はありません。USBはPC等に接続するデバイス側です。CV／ゲートにはモノラル3.5 mmケーブルを使用します。',X,y+3,CW,9.2)

heading(3,'信号の流れと電源','演奏情報はRP2040へ、音声は音源ICからアナログミキサーへ流れます。')
boxlabel(X,135,120,54,'USB MIDI','USB-C / device')
boxlabel(X,204,120,54,'TRS MIDI','6N137で絶縁受信')
boxlabel(X,273,120,54,'CV1 / CV2','保護・バッファ → ADC')
boxlabel(X,342,120,54,'GATE1 / GATE2','保護 → トランジスタ')
boxlabel(X+172,212,135,107,'RP2040','音程・発音・音量を決定<br/>共有8bitバスで音源制御')
for yy in [162,231,300,369]:
 arrow(X+120,yy,X+150,yy);line(X+150,yy,X+150,264,TEAL,1.3)
arrow(X+150,264,X+172,264)
boxlabel(X+355,135,155,58,'SAA1099','L / R アナログ出力')
boxlabel(X+355,211,155,58,'SN1 / SN2','SN1 → L、SN2 → R')
line(X+330,164,X+330,240,TEAL,1.3);arrow(X+307,264,X+330,264)
line(X+330,240,X+330,264,TEAL,1.3)
arrow(X+330,164,X+355,164);arrow(X+330,240,X+355,240)
boxlabel(X+355,306,155,63,'アナログミキサー','TLV9004 / AC結合')
line(X+510,164,X+524,164,ORANGE,1.4)
line(X+510,240,X+524,240,ORANGE,1.4)
line(X+524,164,X+524,337,ORANGE,1.4)
arrow(X+524,337,X+510,337,ORANGE)
# The SN output joins the same mixer path; the colour separates audio/control.
boxlabel(X+355,397,155,54,'LINE OUT','外部アンプへ / stereo')
arrow(X+433,369,X+433,397,ORANGE)
para('青緑：演奏情報と制御　橙：アナログ音声。OLEDとADCはI2Cを共有します。',X,467,CW,9,MUTED)
y=section('USB-Cから2種類の電源をつくる',500)
y=table(['電源','回路と用途'],[
 ['USB VBUS','USB-Cからの約5 V入力。CC1/CC2は各5.1 kΩのRdで接続'],
 ['ロジック 3.2 V','AP2120N-3.2TRG1：RP2040、フラッシュ、OLED、CVバッファ、ADC'],
 ['音源 約5.0 V','TPS61023：USB電圧低下時を含む音源側電源を生成。GPIO29で有効化']], [112,CW-112],y,9.2)
para('ファームウェアはUSBで500 mAを要求し、USB構成後またはCCで1.5 A以上の供給広告を検出後に音源を初期化します。PCのサスペンド時は音源・表示を停止します。突入・未構成・サスペンド電流は実測で確認します。',X,y+1,CW,9.5)

heading(4,'3個の音源ICとステレオ出力','音源ICのレジスタを操作して発音します。RP2040上のソフトウェア音源ではありません。')
y=table(['音源','機能','本基板での役割'],[
 ['U10 / SAA1099','6音程生成器、2ノイズ生成器、音量・エンベロープ制御','左右の音量をIC内部で設定できるステレオ音源'],
 ['U11 / SN76489AN','3音程生成器＋1ノイズ生成器','SN1。アナログ出力を左チャンネルへ混合'],
 ['U12 / SN76489AN','3音程生成器＋1ノイズ生成器','SN2。アナログ出力を右チャンネルへ混合']], [105,203,CW-308],135,9.3)
y=section('ミックスの仕組み',y+11)
y=para('SAA1099のL/Rと、SN1・SN2のモノラル出力をTLV9004で混ぜ、AC結合してライン出力へ送ります。音量調整は音源ICの音量レジスタを使う設計です。SN1/SN2の接続先は基板上で固定されているため、UIで自由な左右パンを追加することはできません。',X,y,CW,10.4)
y=note('接続先：入力インピーダンス10 kΩ以上のアンプ、ミキサー、オーディオインターフェース等。ヘッドホン駆動用アンプとPCM5102は搭載していません。',y+14,58)
y=section('クロックと音程の制約',y+1)
y=table(['項目','設計値／実装上の扱い'],[
 ['クロック','SAA1099：8 MHz、SN76489AN：通常4 MHz、変調250 kHz–4 MHz、RP2040：128 MHzを想定'],
 ['SNの音程','f = clock / (32 × N)、N=1…1023。4 MHz時の最低音程は約122.2 Hz'],
 ['低音の割当','SNの下限と整数分周の誤差を考慮し、低音向けにはSAA1099を割り当てる設計'],
 ['書き込み間隔','SNのREADY端子は未接続。クロックに合わせ16–136 µsの待ちを確保']], [112,CW-112],y,9.3)
y=section('共有データバスのビット順',y+5)
para('GPIO0…7に8bitをまとめて出力するAPIでは、論理バイトをそのまま渡します。PIN_D0…PIN_D7へ個別に書くAPIではビット順を反転します。firmware/sound_bus.hに両方式のヘルパーがあり、768ケースの対応検査を保存しています。',X,y,CW,9.5)

heading(5,'CV／ゲートで2系統を独立に演奏','1 V/octの音程CVとゲートを読み取り、各系統へ予約した音源の声を操作する設計です。')
y=section('CV入力：減衰・バイアス・差動測定',135)
y=para('100 kΩ / 10 kΩの0.1%抵抗でCVを減衰し、バッファ済みの電源中点を加えます。別回路で同じバイアスの基準を作り、ADS1115の差動測定で差し引くため、公称の差動入力はCV / 11になります。BAT54Sのクランプと100 nFのフィルタを入れています。',X,y,CW,10.3)
y=table(['項目','値・意味'],[
 ['入力レンジの設計','CV ±12 Vで測定ノードが電源範囲内に収まる設計。実機評価は未完了'],
 ['ADC','ADS1115 / I2C 0x48、PGA ±2.048 V、AIN0−AIN1とAIN2−AIN3'],
 ['取得速度','ADCの2ch合計は最大860 SPS。交互測定で各ch最大430 SPS、処理時間で低下'],
 ['公称分解能','入力換算 約0.6875 mV/LSB、1 V/octで約0.825 cent/LSB。精度保証値ではない']], [117,CW-117],y+15,9.3)
y=section('ゲート入力：0-5 V／0-10 Vをロジックへ変換',y+8)
y=para('100 kΩの入力抵抗、ベース抵抗、逆方向保護ダイオード、MMBT3904（C20526）で受けます。コレクタを3.2 Vへプルアップするため、外部ゲートがHighのときRP2040入力はLowになる「Active Low」です。閾値と遅延は実機で測定します。',X,y,CW,10.3)
y=section('2点校正と声の予約',y+18)
y=para('系統ごとに既知の0 Vと1 Vを入力して、オフセットとゲインを保存します。0 Vに対応するノートと音源の声・音色を設定できます。CV用の声を予約した場合はMIDIの声割当から除外します。SNの周期ノイズではCVでクロックの音程を変えます。',X,y,CW,10.3)
y=note('用途は音程CVと低速の変調です。ADCの取得速度と入力フィルタにより、オーディオレート変調向けの構成ではありません。',y+16,49)
para('ADC仕様の参照：<link href="https://www.ti.com/lit/ds/symlink/ads1115.pdf" color="#127c7f">TI ADS1115データシート</link>。入力換算の分解能はPGA設定と本基板の1/11減衰比から算出。',X,y+2,CW,8.2,MUTED)

heading(6,'OLEDと3ボタンで設定するUI','v0.2.0-alphaに設定画面を実装。描画コードをPCで検査しています。実OLEDの表示確認は未完了です。')
rect(X,135,CW,60,HexColor('#fbf0e8'),5)
para('表示器：HS96L03W2C03、SSD1315、0.96インチ、128 × 64、白OLED。J5のGND / VCC / SCL / SDAへ接続し、3.2 Vで給電します。I2Cアドレスは0x3Cです。',X+12,145,CW-24,10)
y=table(['ボタン','短押し','長押し'],[
 ['SW5 / −','項目移動・値を減らす','編集以外で全音停止'],
 ['SW6 / 決定','選択・編集の確定','ホームへ戻る'],
 ['SW7 / ＋','項目移動・値を増やす','編集時は連続増加']], [108,201,CW-309],216,9.5)
y=section('メニュー構成',y+8)
y=table(['メニュー','設定する内容'],[
 ['MIX','SAA / SN1 / SN2 / Masterの音量、音色や重ね方'],
 ['CV / GATE 1・2','音源の声、16音色、0 Vノート、有効／無効'],
 ['MIDI','USB／TRSの受信チャンネルと割当、TRS → USB転送、CV声との競合規則'],
 ['PRESET','8枠への保存・読込・初期化。SAVE操作で明示的にフラッシュへ保存'],
 ['CAL','CV1 / CV2を個別に0 V・1 Vで校正'],
 ['SN CLOCK', '共有SNクロックの低下、周期・ランダム・RISE / DIVE変調、MIDI同期'],
 ['ARP / DIAGNOSTICS','クロック同期の音程変化、声の予約、CV生値・エラー・更新遅れ']], [108,CW-108],y,9.3)
y=section('演奏を止めないための実装方針',y+5)
y=para('OLEDは16バイトごとの分割転送で、ADC・音源更新を優先します。MIXは発音中に変更でき、保存操作で設定をまとめて書き込みます。USB／TRSのNote Off追跡と、CV用の声の予約・競合処理を実装しています。',X,y,CW,10)
para('実装済み画面のPCレンダリング：firmware/assets/oled-screens.png。操作・書き込みの詳細：firmware/README.md。',X,y+17,CW,9,MUTED)

heading(7,'手実装と3Dプリントケース','メイン基板の表面実装と、音源IC・OLEDの取り付けを分けた構成です。')
y=table(['取り付け対象','内容'],[
 ['U10','DIP18ソケット＋SAA1099。切欠きは左、1番ピンは左下'],
 ['U11 / U12','DIP16ソケット＋SN76489AN × 2。切欠きは左、1番ピンは左下'],
 ['OLED / J5','27.3 × 27.8 mmモジュール。4本の5 mm M2スペーサーでPCB上へ固定'],
 ['OLED接続','GND / VCC / SCL / SDA。有効長8 mm以上の長ピンまたは短い配線を使用'],
 ['J4','任意のSWDヘッダー。開発・デバッグ用']], [112,CW-112],135,9.3)
y=para('音源IC・OLEDを付ける前にUSB入力と3.2 V電源を確認し、電源を外してソケット・モジュールを取り付けます。ICのピン曲がり、ソケットの向き、OLED端子順と下側部品の高さを確認します。',X,y+6,CW,9.7)
y=image(ROOT/'mechanical/previews/exploded.png',X+250,y+15,CW-250,197)
# Text alongside the exploded diagram.
side_y=y-190
para('ケースの構成',X,side_y,230,13,TEAL)
para('本体 89.8 × 58.8 × 23.0 mm。<br/>上下2分割、押し子3個。<br/>底面からM2 × 10 mmネジ4本で、蓋側のM2インサートに固定。<br/>インサート想定：外径3.5 mm、長さ3.0 mm、下穴3.2 mm。',X,side_y+27,225,9.5)
y=para('CAD分解図。部品外形は機械的な参考モデルです。',X+250,y+2,CW-250,7.8,MUTED)
y=section('ケースの組み立て順',y+19)
y=para('1. 試し刷りパーツで下穴と押し子ガイドを確認し、蓋へインサートを圧入。<br/>2. 音源ICとOLEDを取り付けた基板を底ケースの4支柱へ載せる。<br/>3. 蓋へ押し子3個を入れ、上下を合わせて底面からネジ止めする。',X,y,CW,9.6)
para('音源ICとOLEDは開口から直接見せます。追加の透明板、導光材、ケース刻印は使いません。OLEDの5 mmスペーサーは基板側の取り付けを維持します。',X,y+14,CW,9.3)

heading(8,'開発者向け情報と検証の範囲','現状の成果物と、実機でこれから確認する事項を整理します。')
y=table(['GPIO','用途'],[
 ['0…7','共有データ。PIN_D7=0 … PIN_D0=7'],
 ['8…12 / 25','WR、SAA CS、SN1 CS、SN2 CS、SAA A0 / SAA DTACK'],
 ['13 / 14 / 16','TRS MIDI RX / SAA clock / SN共通clock'],
 ['15 / 17','GATE1 / GATE2（Active Low）'],
 ['18 / 19 / 20…22','I2C SDA / SCL / −・決定・＋ボタン'],
 ['26 / 27 / 28 / 29','CC1 ADC / CC2 ADC / 状態LED / SOUND_ENABLE'],
 ['23 / 24','空きGPIO。基板上では未接続']], [113,CW-113],135,8.7)
y=section('設計検査と未完了の項目',y+3)
y=para('検査済み：DRC 0、未接続0、ERC 0、回路図との不一致0。152フットプリントの配置・端子接続、音源バス768ケース、ケース干渉とSTL形状を検査しています。PCBA対象は141点、BOMは41行・40品種です。',X,y,CW,9.4)
y=para('ソフトウェア：UF2ビルド、ASan／UBSan付きPCテストを通過。実機ではUSB列挙・電流、音声レベル・雑音、CV精度・校正、ゲート応答、温度、印刷後の嵌め合いが未確認です。',X,y+10,CW,9.4)
y=section('初回起動・書き込みの確認順',y+15)
y=para('3.2 V電源確認 → 電源OFFで手実装 → UF2を書き込み、消音・クロック・音源電源を確認 → 音源ごとの試験音 → MIDI → CV／ゲート → 校正・保存の順で検証します。実機評価用UF2をfirmware/distに収録しています。',X,y,CW,9.2)
y=para('JP1=BOOT、JP2=RESETは裏面のサービス用銅パッドです。BOOTを短絡保持し、RESETを短絡して解放するとUSB書き込みモードに入る構成です。通常操作の3ボタンとは別です。v0.2.0-alphaは−と＋を押しながらUSB接続しても書き込みモードに入ります。',X,y+9,CW,9.2)
y=section('設計ファイル・参照先',y+13)
links=[('GitHub: Bongorian/chiptune-usb-cv','https://github.com/Bongorian/chiptune-usb-cv'),('回路仕様・操作設計','https://github.com/Bongorian/chiptune-usb-cv/blob/main/docs/USB-CV-design-notes.txt'),('ファームウェア・操作資料','https://github.com/Bongorian/chiptune-usb-cv/tree/main/firmware'),('製造データ・手実装一覧','https://github.com/Bongorian/chiptune-usb-cv/tree/main/outputs/jlc-one'),('ケースCAD・印刷手順','https://github.com/Bongorian/chiptune-usb-cv/tree/main/mechanical')]
for label,url in links:
 y=para(f'<link href="{url}" color="#127c7f">{label}</link>',X,y,CW,8.5)+3
para('基準：配線済みRev C、2026-10-05の設計検査、2026-10-12時点のリポジトリ。音源仕様はPhilips SAA1099とTI SN76489ANのメーカー資料を確認。ファームウェアはv0.2.0-alpha。実機動作とUSB適合性は未検証です。',X,y+4,CW,7.8,MUTED)
assert page==8
C.save()
inputs=['routed/Chiptune_USB_CV.kicad_pcb','routed/design.json','firmware/pinmap.h','firmware/sound_bus.h','firmware/README.md','firmware/src/main.c','firmware/src/engine.c', 'firmware/src/sn_clock.c', 'firmware/pio/sn_clock.pio','firmware/dist/Verification.json','docs/USB-CV-design-notes.txt','docs/routing-notes.txt','mechanical/parameters.json','mechanical/mechanical_verification.json','outputs/routing-verification.json','outputs/bus-encoding-verification.json','outputs/jlc-one/reference/Hand_Assembly.csv']
manifest=dict(document='Chiptune USB/CV Japanese board guide',date='2026-10-12',pages=8,pdf_sha256=hashlib.sha256(PDF.read_bytes()).hexdigest(),source_files_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs},layout_paragraph_bounds=bounds)
(OUT/'Guide_Verification.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(PDF)
print('8 pages generated; text bounds remain inside the content area.')
