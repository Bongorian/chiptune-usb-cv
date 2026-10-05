#!/usr/bin/env python3
"""Export reviewed four-layer copper and an assembly-upload draft; never submit orders."""
import csv,json,subprocess,collections,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
checks=json.loads((root/'outputs/routing-verification.json').read_text());assert checks['complete']
board=root/'routed/Chiptune_USB_CV.kicad_pcb';out=root/'outputs/jlcpcb';gerber=out/'gerber';gerber.mkdir(parents=True,exist_ok=True)
subprocess.run(['kicad-cli','pcb','export','gerbers','--layers','F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.SilkS,B.SilkS,Edge.Cuts','--use-drill-file-origin','--subtract-soldermask','-o',str(gerber),str(board)],check=True)
subprocess.run(['kicad-cli','pcb','export','drill','--drill-origin','plot','--excellon-separate-th','--excellon-oval-format','route','--generate-report','--report-path',str(out/'drill-report.txt'),'-o',str(gerber),str(board)],check=True)
subprocess.run(['kicad-cli','pcb','export','pos','--format','csv','--units','mm','--side','front','--use-drill-file-origin','-o',str(out/'kicad-positions.csv'),str(board)],check=True)
rows=list(csv.DictReader((root/'outputs/engineering_bom.csv').open()));assembly=[r for r in rows if r['LCSC'] and r['Reference']!='J5'];selected={r['Reference'] for r in assembly};group=collections.defaultdict(list)
for row in assembly:group[(row['LCSC'],row['Value'])].append(row)
with (out/'BOM.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['Comment','Designator','Footprint','LCSC Part #'])
 for (code,value),rr in group.items():w.writerow([value,','.join(r['Reference'] for r in rr),rr[0]['Footprint'],code])
with (out/'CPL.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['Designator','Mid X','Mid Y','Layer','Rotation'])
 for row in csv.DictReader((out/'kicad-positions.csv').open()):
  if row['Ref'] in selected:w.writerow([row['Ref'],row['PosX'],row['PosY'],'top',row['Rot']])
with zipfile.ZipFile(out/'Chiptune_USB_CV_4layer_Gerber.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in sorted(gerber.iterdir()):z.write(f,f.name)
manual=[r for r in rows if r['Reference'] not in selected]
(out/'README.txt').write_text('JLCPCBアップロード用データ — 4層版\n\n基板: 85×54 mm / R3 / 4層 / FR4 1.6 mm / 外層1 oz・内層0.5 oz。\n推奨発注条件: Economic PCBA、表面1面、緑レジスト、標準積層。\n順序: F.Cu / In1.Cu / In2.Cu(GND) / B.Cu。\nGerber/ドリル/CPLは共通の基板左下原点。CPL座標はmm、回転はKiCad出力値。\n\nBOM.csv/CPL.csvはアップロード用の下書き。発注前にJLCPCBプレビューで部品選択、\n実装位置、極性・1番ピン・回転、USBとジャックの開口方向を確認すること。\n部品の在庫・価格・JLCPCBライブラリに対する回転補正は未確認。発注・アップロードはしていない。\n\n手実装・別途購入: '+', '.join(r['Reference']+' '+r['Value'] for r in manual)+'\nOLEDはC5248080を別途手実装、DIP18/DIP16ソケットと支給音源ICも手実装。\n\nPCB DRC 0、未接続0、回路図一致0、ERC 0。USB 90Ωの管理積層保証はない。\n実機ファームウェア、USB電源電流・突入、音声、CV校正の試作確認は未完了。\n')
print('PCBA components',len(assembly),'BOM groups',len(group),'manual entries',len(manual))
