#!/usr/bin/python3
"""Actual CAD placement beside conceptual block flows; no fake copper paths."""
from pathlib import Path
import json
from review_render import drawing, rect, text, fps, mm

metrics=json.loads(Path('outputs/review/flow-metrics.json').read_text())
colors={'U':'#265b99','M':'#894978','C':'#996027','D':'#416b47','A':'#975544','P':'#666379'}
s='<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 132 84" width="2112" height="1344">'
s+=rect(0,0,132,84,'#f5f6f3','#f5f6f3',0,0)
s+=text('信号の流れを整理した配置案',4,4.6,2.3,'#233f34','start',500)
s+=text('85 × 54 mm / 配線前 / 表示器を外した状態',4,7.9,1.2,'#576a60','start')
s+='<g transform="translate(4,10)">'+drawing()
for k,r,dx,dy in [('U','U9',0,-3.2),('M','U13',0,7),('C','U16',0,-4),('D','U1',0,4.8),('A','U14',0,-4.5),('P','L1',0,-4)]:
    f=fps[r];x=mm(f.GetPosition().x)-100+dx;y=mm(f.GetPosition().y)-100+dy
    s+=f'<circle cx="{x}" cy="{y}" r="1.05" fill="{colors[k]}" stroke="#fff" stroke-width=".12"/>'
    s+=text(k,x,y+.42,1.2,'#fff',weight=500)
s+='</g>'
cards=[
 ('U','USB MIDI','USB-C → ESD保護 → RP2040','D+/D−の直列抵抗はMCUの近く。'),
 ('M','TRS MIDI IN','ジャック → 保護 → 6N137 → RP2040','受信回路を左のジャック側へ移動。'),
 ('C','CV 1 / CV 2','減衰・保護 → バッファ → ADC → MCU','高インピーダンスの区間を小さくまとめる。'),
 ('D','音源バス / クロック','RP2040 → SAA1099・SN76489 × 2','データ端子を音源側へ。クロック抵抗は送信側。'),
 ('A','アナログ音声','3個の音源 → ミキサー → LINE OUT','帰還部品をアンプ入力・出力の近くへ。'),
 ('P','音源用電源','USB給電 → 昇圧 → 5V音源レール','高速スイッチ部をCV測定・ミキサーから離す。'),
]
for i,(k,title,flow,note) in enumerate(cards):
    y=10+i*9.2
    s+=rect(92,y,36,8.2,'#ffffff','#d5ded8',.08,.6)
    s+=f'<circle cx="94" cy="{y+2}" r="1.05" fill="{colors[k]}"/>'+text(k,94,y+2.4,1.2,'#fff',weight=500)
    s+=text(title,96,y+2.5,1.35,'#233f34','start',500)
    s+=text(flow,93.3,y+4.8,1,'#344f41','start')
    s+=text(note,93.3,y+6.9,.91,'#617267','start')
s+=text('ゲート入力は各ジャック脇で保護・レベル変換し、MCUへ。',4,68,1.25,'#40574a','start')
changes=metrics['change_percent']
s+=text(f"端子間距離の概算：MIDI→MCU {changes['midi_to_mcu']:.0f}% / CV測定点 {changes['cv_high_impedance']:.0f}% / ミキサー帰還 {changes['mixer_feedback']:.0f}%",4,71,1.25,'#40574a','start')
s+=text('配線方針：USBは連続したGND上のペア配線、CV測定点・水晶・帰還部は局所配線。',4,74,1.2,'#40574a','start')
s+=text('配置DRC 0件 / 回路ERC 0件 / 配線・ビア 0本。配置の確認後に配線します。',4,77,1.2,'#40574a','start')
s+=text('概算は各ネットの端子間マンハッタン最小全域木。実配線長や電気性能の保証ではありません。',4,80,1.05,'#617267','start')
s+='</svg>'
Path('outputs/review/signal-flow-placement.svg').write_text(s)
print('Wrote placement and conceptual signal-flow review')
