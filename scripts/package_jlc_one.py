#!/usr/bin/env python3
"""Package the routed revision for JLCONE, without uploading or ordering."""
import collections
import csv
import hashlib
import json
import math
import re
import shutil
import subprocess
import zipfile
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'outputs/jlc-one'
BOARD = ROOT / 'routed/Chiptune_USB_CV.kicad_pcb'


def run(*args):
    subprocess.run(list(map(str, args)), check=True)


def write_csv(path, headers, rows):
    with path.open('w', encoding='utf-8', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(headers)
        writer.writerows(rows)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    checks = json.loads((ROOT / 'outputs/routing-verification.json').read_text())
    assert checks['complete'] and checks['copper_layers'] == 4
    assert all(checks[key] == 0 for key in (
        'drc_violations', 'unconnected', 'erc_violations', 'schematic_parity_issues'))
    for folder in ['upload', 'reference', 'checks', 'gerber']:
        (OUT / folder).mkdir(parents=True, exist_ok=True)
    gerber = OUT / 'gerber'
    # Conventional RS-274X, no X2/netlist attributes; separate PTH/NPTH drills.
    run('kicad-cli', 'pcb', 'export', 'gerbers', '--layers',
        'F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.Paste,B.Paste,F.SilkS,B.SilkS,Edge.Cuts',
        '--no-x2', '--no-netlist', '--use-drill-file-origin', '--subtract-soldermask',
        '-o', gerber, BOARD)
    run('kicad-cli', 'pcb', 'export', 'drill', '--drill-origin', 'plot',
        '--excellon-separate-th', '--excellon-oval-format', 'route', '--generate-report',
        '--report-path', OUT / 'reference/Drill_Report.txt', '-o', gerber, BOARD)
    gerber_files = sorted(p for p in gerber.iterdir() if p.suffix != '.gbrjob')
    assert len(gerber_files) == 13
    assert {'.gtl', '.g1', '.g2', '.gbl'}.issubset({p.suffix for p in gerber_files})
    for p in gerber_files:
        if p.suffix != '.drl':
            assert '%TF.' not in p.read_text() and '%TO.' not in p.read_text()
    gerber_zip = OUT / 'upload/Chiptune_Gerber.zip'
    with zipfile.ZipFile(gerber_zip, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in gerber_files:
            z.write(p, p.name)

    catalog = json.loads((ROOT / 'research/catalog.json').read_text())
    for p in (ROOT / 'research').glob('catalog-C*.json'):
        data = json.loads(p.read_text())
        catalog[p.stem.removeprefix('catalog-')] = data.get('result', data)
    rows = list(csv.DictReader((ROOT / 'outputs/engineering_bom.csv').open()))
    assembly = [r for r in rows if r['LCSC'] and r['Reference'] != 'J5']
    selected = {r['Reference'] for r in assembly}
    assert len(selected) == len(assembly) == 141
    group = collections.defaultdict(list)
    for r in assembly:
        assert re.fullmatch(r'C\d+', r['LCSC'])
        # FB1 is deliberately a 10-ohm resistor; preserve its PCB designator.
        # Keep it on its own BOM row to avoid merging different refdes prefixes.
        group[(r['LCSC'], re.match(r'[A-Z]+', r['Reference'])[0])].append(r)
    bom_rows = []
    for (code, _), members in group.items():
        part = catalog[code]
        assert part and part['productModel'] and part['encapStandard']
        assert len(members) <= 200
        bom_rows.append([part['productModel'], ','.join(r['Reference'] for r in members),
                         part['encapStandard'], code])
    bom = OUT / 'upload/Chiptune_BOM.csv'
    write_csv(bom, ['Comment', 'Designator', 'Footprint', 'LCSC Part #'], bom_rows)

    native_positions = OUT / 'reference/KiCad_Positions.csv'
    run('kicad-cli', 'pcb', 'export', 'pos', '--format', 'csv', '--units', 'mm',
        '--side', 'front', '--use-drill-file-origin', '-o', native_positions, BOARD)
    pos = list(csv.DictReader(native_positions.open()))
    corrections = json.loads((ROOT / 'routed/jlc-placement-corrections.json').read_text())
    cpl_rows = []
    for r in pos:
        if r['Ref'] not in selected:
            continue
        correction = corrections.get(r['Ref'], {})
        cpl_rows.append([r['Ref'], f'{float(r["PosX"])+correction.get("dx_mm",0):.4f}',
                         f'{float(r["PosY"])+correction.get("dy_mm",0):.4f}', 'Top',
                         f'{(float(r["Rot"])+correction.get("rotation_offset",0))%360:.1f}'])
    cpl = OUT / 'upload/Chiptune_CPL.csv'
    write_csv(cpl, ['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'], cpl_rows)
    bom_refs = [ref for row in bom_rows for ref in row[1].split(',')]
    assert len(bom_refs) == len(set(bom_refs)) == 141
    assert set(bom_refs) == {r[0] for r in cpl_rows} == selected
    assert len(cpl_rows) == len({r[0] for r in cpl_rows}) == 141
    assert all(0 <= float(r[1]) <= 85 and 0 <= float(r[2]) <= 54 for r in cpl_rows)
    min_distance = min(math.hypot(float(a[1])-float(b[1]), float(a[2])-float(b[2]))
                       for i, a in enumerate(cpl_rows) for b in cpl_rows[i+1:])
    assert min_distance > 0.2

    manual = [r for r in rows if r['Reference'] not in selected]
    write_csv(OUT / 'reference/Hand_Assembly.csv',
              ['Designator', 'Component', 'LCSC reference only', 'Instructions'],
              [[r['Reference'], r['Value'], r['LCSC'], r['Notes']] for r in manual])
    write_csv(OUT / 'reference/Assembly_Details.csv',
              ['Designator', 'Circuit role or value', 'Manufacturer part number',
               'Package', 'LCSC Part #', 'KiCad footprint', 'Notes'],
              [[r['Reference'], r['Value'], catalog[r['LCSC']]['productModel'],
                catalog[r['LCSC']]['encapStandard'], r['LCSC'], r['Footprint'], r['Notes']]
               for r in assembly])
    display_board = ROOT / 'outputs/review/jlc-one-display-only.kicad_pcb'
    run('/usr/bin/python3', ROOT / 'scripts/make_assembly_reference.py', BOARD,
        display_board)
    for side, layers, extra in [
        ('Top', 'F.Fab,Edge.Cuts', []),
        ('Bottom', 'B.Fab,Edge.Cuts', ['--mirror'])]:
        run('kicad-cli', 'pcb', 'export', 'svg', '--layers', layers,
            '--mode-single', '--page-size-mode', '2', '--exclude-drawing-sheet',
            '--black-and-white', *extra, '-o', OUT / f'reference/Assembly_{side}.svg', display_board)
    shutil.copy2(ROOT / 'outputs/review/routing-traces-all.png',
                 OUT / 'reference/Routing_Overview.png')
    for name in ['routing-verification.json', 'drc-routed.json', 'erc-routing.json',
                 'bus-encoding-verification.json']:
        shutil.copy2(ROOT / 'outputs' / name, OUT / 'checks' / name)
    for name in ['USB-CV-design-notes.txt', 'routing-notes.txt']:
        shutil.copy2(ROOT / 'docs' / name, OUT / 'reference' / name)
    shutil.copy2(ROOT / 'outputs/routing-widths.csv', OUT / 'reference/Net_Widths.csv')
    for p in sorted((ROOT / 'outputs/assembly-review').iterdir()):
        if p.is_file():
            shutil.copy2(p, OUT / ('checks' if p.suffix == '.json' else 'reference') / p.name)
    shutil.copy2(ROOT / 'routed/jlc-placement-corrections.json',
                 OUT / 'reference/JLC_Placement_Corrections.json')
    with zipfile.ZipFile(OUT / 'reference/KiCad_Routed_Project.zip', 'w',
                         zipfile.ZIP_DEFLATED) as z:
        for p in sorted((ROOT / 'routed').rglob('*')):
            if p.is_file() and p.suffix not in {'.kicad_prl', '.lck'}:
                z.write(p, 'Chiptune_USB_CV/' + str(p.relative_to(ROOT / 'routed')))
    (OUT / 'reference/Order_Specification.txt').write_text(
        'CHIPTUNE USB/CV — routed four-layer revision, prototype build\n\n'
        'PCB: single board, 85 x 54 mm, corner radius 3 mm; FR-4, 1.6 mm.\n'
        'Four copper layers: F.Cu / In1.Cu (signal) / In2.Cu (GND) / B.Cu.\n'
        'Target copper: outer 1 oz / inner 0.5 oz, standard manufacturer stackup.\n'
        'Solder mask: green. Surface finish: lead-free HASL as a cost-conscious\n'
        'option; select available finish in the quote. No controlled impedance specified.\n'
        'Assembly: TOP ONLY, 141 placements. Quote Economic PCBA first; actual\n'
        'part/service eligibility must be checked in JLCONE. Board quantity and\n'
        'assembled quantity are customer order choices, not encoded in the files.\n'
        'Files describe ONE PCB, not a panel. Any manufacturing rails, tooling holes,\n'
        'fiducials and panel repetition must be handled in the manufacturer preview.\n'
        'RS-274X Gerbers, separate Excellon PTH/NPTH drill files with routed slots.\n'
        'CPL: mm, lower-left board origin, +X right, +Y up, Top side.\n'
        'Explicit supplier-library rotation/anchor offsets applied; see Placement_Audit.csv.\n'
        'These offsets fit supplier footprint pads/pegs and still need fresh online preview.\n'
        'All board component coordinates and orientations are preserved.\n\n'
        'J1 USB-C TYPE-C-31-M-12 / C165948: source from JLCPCB public stock and\n'
        'assemble on TOP, one per board. Select JLCPCB source instead of My Parts\n'
        'in component matching. BOM/CPL do not encode the account inventory source.\n\n'
        'Do not fit J4, J5, U10, U11, U12, JP1, JP2 at JLCPCB; see hand assembly list.\n'
        'Fit user-supplied SAA1099 DIP18 and two SN76489AN DIP16 via sockets later.\n'
        'OLED module HS96L03W2C03 (C5248080) is purchased/fitted separately using\n'
        '5 mm M2 spacers. J4 SWD header optional; JP1/JP2 are rear service pads,\n'
        'normally OPEN, and are not discrete parts. Mounting holes are not placements.\n'
        'FB1 is a 10-ohm 0603 resistor, C22859 (same part as R4), not a ferrite bead.\n'
        'FB1 has a separate BOM row to preserve reference prefixes; a duplicate-part\n'
        'matching warning for C22859 can be reviewed using Assembly_Details.csv.\n\n'
        'Verification: KiCad DRC 0, unconnected 0, ERC 0, schematic parity 0.\n'
        'Replacement C20526 and C2286 stock/Basic classification checked on 2026-10-05.\n'
        'Not yet verified: all remaining stock, final quote, live JLCONE model/reference\n'
        'frame, online assembly preview, prototype electrical tests.\n'
        'Review pin 1, polarity, USB/jack openings against Assembly_Top.svg in the\n'
        'online preview BEFORE approving fabrication/assembly. Firmware programming\n'
        'and functional testing are not supplied with this manufacturing package.\n', encoding='utf-8')
    (OUT / 'README.txt').write_text(
        'JLCONE提出用パッケージ — CHIPTUNE USB/CV 4層版\n\n'
        '外側のまとめZIPは保管・受け渡し用です。展開後、upload/内の3点を\n'
        'JLCONEの対応する入力欄にそれぞれ指定してください。\n'
        '1. 基板データ: upload/Chiptune_Gerber.zip（このZIPは展開せず提出）\n'
        '2. 部品表: upload/Chiptune_BOM.csv\n'
        '3. 実装座標: upload/Chiptune_CPL.csv\n\n'
        f'表面実装141点、BOM {len(bom_rows)}行、LCSC部品{len(set(r["LCSC"] for r in assembly))}種類。\n'
        '独自KiCad名ではなく、LCSCの保存済みカタログにあるメーカー型番と\n'
        'パッケージ名を使用。交換品C20526/C2286のBasic区分と在庫を2026-10-05に確認。\n'
        '残りの全品目の最新在庫・発注総額は未確認です。\n'
        'reference/Assembly_Details.csvで回路上の用途とメーカー部品を照合できます。\n\n'
        'USB-C J1(C165948)はJLCPCB公開在庫から調達・表面実装する対象です。\n'
        'JLCONEの部品選択画面で、J1の調達元をMy PartsからJLCPCB在庫へ選び直す。\n'
        'BOM/CPLには調達元の指定欄がないため、ファイルだけで切り替わるとは限りません。\n'
        '1基板につき1個。注文画面の実装対象にJ1が選択されていることを確認する。\n\n'
        '発注設定: 85×54 mm、角R3、FR-4 1.6 mm、4層、表面1面実装、緑レジスト。\n'
        'Economic PCBAで見積もり、部品の対応可否・費用を画面で確認してください。\n'
        '数量は希望する枚数を画面で設定してください。パネルではなく単体基板のデータです。\n'
        '詳しい製造条件: reference/Order_Specification.txt\n\n'
        '手実装: SAA1099＋SN76489AN×2とDIPソケット、OLEDモジュールと接続、\n'
        '任意のSWDヘッダー。BOOT/RESETは通常開放の背面サービスパッドです。\n'
        'reference/Hand_Assembly.csvのLCSC欄は別途購入用で、PCBA対象ではありません。\n'
        '配置確認図: reference/Assembly_Top.svg（表面）、Assembly_Bottom.svg（裏面反転表示）。\n'
        '配置確認図はFabの参照図で、重複ラベルを非表示にした閲覧用コピーから出力。\n'
        '基板の部品座標・向きは原本と同一で、基板に印刷されるシルクではありません。\n'
        'Routing_Overview.pngは配線閲覧図です。製造データには使用しないでください。\n'
        'KiCad_Routed_Project.zipは配線済み原本とライブラリの保存用です。\n\n'
        '確認済み: DRC・未接続・ERC・回路図一致は各0件。BOM/CPLの全141点一致、\n'
        '座標範囲、重複参照番号、ZIP CRC、4銅箔層＋PTH/NPTHを照合済み。\n'
        'checks/Package_Verification.jsonおよびSHA256SUMS.txtに検証結果とハッシュを保存。\n'
        'アップロード後に必要: 実装プレビューで1番ピン・極性・回転・USB/ジャック開口方向\n'
        'を確認。回転・中心補正はライブラリのパッド番号と位置決め穴に基づく補正候補。\n'
        'ライブのJLCONEモデル上での一致は未確認です。過去の手動調整を重ねず、\n'
        '修正版BOM/CPLを新規に読み込んでPlacement_Pin1.svgと照合してください。\n'
        '全IC・トランジスタ・ダイオードとジャックの照合記録: Placement_Audit.csv。\n'
        '今回の部品変更と費用比較: Basic_Parts_Review.csv、Assembly_Review.txt。\n'
        'FB1は10Ω抵抗(C22859)でR4と同じ部品のため、同一部品複数行の警告が出る\n'
        '可能性があります。回路上のFB1という参照番号は維持しています。\n'
        'ファームウェアと実機試験（電流・突入、音声、CV校正）は未完了です。\n'
        'このパッケージでアップロード・発注は行っていません。\n\n'
        '公式案内:\nhttps://jlcpcb.com/help/article/jlcone-desktop-app\n'
        'https://jlcpcb.com/help/article/advice-for-bom-and-cpl-files-preparation\n',
        encoding='utf-8')
    with zipfile.ZipFile(gerber_zip) as z:
        assert z.testzip() is None
    verification = {
        'created_at': datetime.now(ZoneInfo('Asia/Tokyo')).isoformat(timespec='seconds'),
        'board_sha256': sha256(BOARD), 'copper_layers': 4,
        'gerber_zip_files': len(gerber_files), 'gerber_format': 'RS-274X',
        'assembly_components': len(cpl_rows), 'bom_rows': len(bom_rows),
        'distinct_lcsc_parts': len(set(r['LCSC'] for r in assembly)),
        'bom_cpl_exact_reference_match': True, 'duplicate_designators': 0,
        'cpl_units': 'mm', 'cpl_origin': 'board lower-left',
        'cpl_bounds_within_85x54': True, 'minimum_centroid_distance_mm': min_distance,
        'gerber_zip_crc_ok': True, 'source_checks': checks,
        'supplier_inventory_checked': False, 'replacement_parts_inventory_checked': ['C20526','C2286'],
        'usb_c_public_inventory_checked': 'C165948',
        'usb_c_requested_source': 'JLCPCB public stock',
        'account_inventory_source_changed': False,
        'supplier_library_reference_offsets_applied': sorted(corrections),
        'supplier_rotation_preview_checked': False,
        'uploaded_or_ordered': False,
    }
    (OUT / 'checks/Package_Verification.json').write_text(
        json.dumps(verification, ensure_ascii=False, indent=2) + '\n')
    included = [OUT / 'README.txt'] + sorted(p for folder in ['upload', 'reference', 'checks']
                                          for p in (OUT / folder).rglob('*') if p.is_file())
    (OUT / 'SHA256SUMS.txt').write_text(''.join(
        f'{sha256(p)}  {p.relative_to(OUT)}\n' for p in included))
    included.append(OUT / 'SHA256SUMS.txt')
    final = ROOT / 'outputs/Chiptune_JLC_ONE_submission.zip'
    with zipfile.ZipFile(final, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in included:
            z.write(p, 'Chiptune_JLC_ONE/' + str(p.relative_to(OUT)))
    with zipfile.ZipFile(final) as z:
        assert z.testzip() is None
    print(json.dumps(verification, ensure_ascii=False, indent=2))
    print('Submission package:', final, final.stat().st_size, 'bytes')


if __name__ == '__main__':
    main()
