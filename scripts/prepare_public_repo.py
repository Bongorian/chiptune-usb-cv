#!/usr/bin/python3
"""Build a curated public snapshot without personal attachments or web caches."""
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'publication/chiptune-usb-cv'
DEST.mkdir(parents=True, exist_ok=True)


def copy_file(src, relative=None):
    target = DEST / (relative or src.relative_to(ROOT))
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, target)


for folder in ['routed', 'hardware', 'firmware', 'scripts', 'docs', '.github']:
    for path in (ROOT / folder).rglob('*'):
        if path.is_file() and not set(path.parts).intersection({'__pycache__', '.deps', '.tools', '.build', 'build'}) and path.suffix not in {'.pyc', '.kicad_prl', '.lck'}:
            copy_file(path)
copy_file(ROOT / '.gitattributes')
for name in ['engineering_bom.csv', 'routing-verification.json', 'drc-routed.json',
             'erc-routing.json', 'bus-encoding-verification.json', 'routing-widths.csv']:
    copy_file(ROOT / 'outputs' / name)
for folder in ['outputs/assembly-review', 'outputs/jlc-one']:
    for path in (ROOT / folder).rglob('*'):
        if path.is_file() and 'gerber' not in path.relative_to(ROOT / folder).parts:
            copy_file(path)
for name in ['routing-traces-all.png', 'routing-traces-front.png', 'routing-traces-back.png',
             'routing-inner1.png', 'routing-ground.png', 'signal-flow-placement.png',
             'rp2040-closeup.png', 'chiptune-ui.html', 'ui-preview.html']:
    copy_file(ROOT / 'outputs/review' / name)
copy_file(ROOT / 'variants/jlc-one-before-basic-review/routed/Chiptune_USB_CV.kicad_pcb')

# Enclosure deliverables: exclude vendor drawings, temporary logs and render meshes.
mechanical = ROOT / 'mechanical'
for path in mechanical.iterdir():
    if path.is_file() and (path.suffix in {'.py', '.json', '.md', '.txt'} or path.name == '.gitignore'):
        copy_file(path)
for path in (mechanical / 'exports').iterdir():
    if path.is_file() and (path.suffix in {'.stl', '.step', '.blend', '.zip'} or path.name == 'SHA256SUMS.txt'):
        copy_file(path)
for name in ['assembled.png', 'top.png', 'exploded.png', 'base_inside.png',
             'mechanical_drawing.svg', 'mechanical_drawing.png', 'mechanical_drawing.pdf']:
    copy_file(mechanical / 'previews' / name)

# Public board guide and its source-verification record.
for name in ['Chiptune_USB_CV_Guide_JA.pdf', 'Guide_Verification.json']:
    copy_file(ROOT / 'output/pdf' / name)

# Only portable public part attributes, not complete scraped responses or signed URLs.
catalog = json.loads((ROOT / 'research/catalog.json').read_text())
for path in (ROOT / 'research').glob('catalog-C*.json'):
    data = json.loads(path.read_text())
    catalog[path.stem.removeprefix('catalog-')] = data.get('result', data)
public_catalog = {code: {key: record[key] for key in ['productCode', 'productModel', 'encapStandard',
                  'brandNameEn', 'edaSvgInfo'] if key in record}
                  for code, record in catalog.items() if isinstance(record, dict)}
(DEST / 'research').mkdir(exist_ok=True)
(DEST / 'research/catalog.json').write_text(json.dumps(public_catalog, ensure_ascii=False, indent=2)+'\n')
for path in (ROOT / 'research/jlc-review').glob('*-pcb.svg'):
    copy_file(path)

# Keep the current design notes consistent with the Basic transistor substitution.
notes = DEST / 'docs/USB-CV-design-notes.txt'
notes.write_text(notes.read_text().replace('MMBT3904-7-F C94514', 'MMBT3904(RANGE:100-300) C20526'))

# Local paths are useful in diagnostics, but are not part of the public design.
for path in DEST.rglob('*'):
    if path.is_file() and path.suffix in {'.json', '.txt', '.csv', '.py', '.cpp', '.html'}:
        original = path.read_text()
        cleaned = re.sub(r'/home/[^/]+/\.codex/attachments/', '[local attachment]/', original.replace(str(ROOT) + '/', ''))
        cleaned = re.sub(r'/Users/[^/]+/Pictures/Screenshots/', '[user screenshot]/', cleaned)
        if cleaned != original:
            path.write_text(cleaned)

# Preserve only manufacturing-facing material; do not publish customer quote history.
for relative in ['outputs/assembly-review/Basic_Parts_Review.csv',
                 'outputs/jlc-one/reference/Basic_Parts_Review.csv']:
    path = DEST / relative
    import csv
    rows = list(csv.DictReader(path.open()))
    fields = ['References', 'Revised LCSC', 'Revised library', 'Decision reason', 'Source']
    for row in rows:
        if row['References'] == 'U2':
            row['Decision reason'] = '同じ208milのBasic候補も確認したが、価格と互換確認を踏まえて現行品を維持'
        if row['References'] == 'J1':
            row['Decision reason'] = 'C165948を調達・実装。My Partsに使用可能在庫がある場合はその在庫も利用可能'
    with path.open('w', newline='') as handle:
        writer = csv.DictWriter(handle, fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(rows)

review = '''実装レビュー・調達方針（2026-10-05）
D2: 赤0603 LED KT-0603R / C2286、Basic。R55=2.2kを維持。
Q3/Q4: MMBT3904(RANGE:100-300) / C20526、Basic。SOT-23 B1/E2/C3を維持。
USB-C J1: TYPE-C-31-M-12 / C165948、調達・表面実装対象、1基板1個。
My Partsに使用可能在庫がある場合はその在庫を使用できる。
新規調達時はJLCPCB公開在庫から同じC165948を選択する。
調達元の選択は注文画面の設定で、BOM/CPLには含まれない。

部品の基板座標・向き・パッド・配線・ビアはBasic交換前と同一。
CPLには公開ライブラリに基づく部品別の角度・中心座標補正を適用。
全9個の実装ICについて公開ライブラリの全パッド番号を照合。
ジャックはメーカー接点図と位置決めペグ、USB-Cは8本の信号端子から照合。
Placement_Audit.csvに照合記録、Placement_Pin1.svg/.pngにピン1・K位置を表示。
新しい実装プレビューの外形・リード配置は整合。U1/U13/U16のピン1印は目視一致。
U2/U3/U9/U14/U15のピン1印は画像から独立して判別できず、製造前に確認する。
ビュワーの色付き点は必ずしも1番ピンを意味しない。

DRC=0、未接続=0、回路図パリティ=0、ERC=0。実装対象141点。
仕様が変わる水晶・精密抵抗・アナログICの安価品への交換は行っていない。
実機ファームウェア・電源突入測定・音声検証・CV校正は未完了。
在庫・価格は変動するため、注文時に再確認する。
https://jlcpcb.com/partdetail/C2286
https://jlcpcb.com/partdetail/C20526
https://jlcpcb.com/partdetail/C165948
https://jlcpcb.com/help/article/how-to-use-my-own-parts-for-pcb-assembly-order
https://jlcpcb.com/help/article/pcb-assembly-faqs-part-2
'''
for relative in ['outputs/assembly-review/Assembly_Review.txt', 'outputs/jlc-one/reference/Assembly_Review.txt']:
    (DEST / relative).write_text(review)

instructions = DEST / 'outputs/jlc-one/README.txt'
instructions.write_text(instructions.read_text().replace(
    'JLCONEの部品選択画面で、J1の調達元をMy PartsからJLCPCB在庫へ選び直す。',
    'My Partsに使用可能在庫があれば利用し、新規調達時はJLCPCB公開在庫から同じC165948を選ぶ。'))

# Regenerate nested project ZIP so the published native sources match the archive.
native_zip = DEST / 'outputs/jlc-one/reference/KiCad_Routed_Project.zip'
with zipfile.ZipFile(native_zip, 'w', zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((DEST / 'routed').rglob('*')):
        if path.is_file():
            archive.write(path, 'Chiptune_USB_CV/' + str(path.relative_to(DEST / 'routed')))

(DEST / '.gitignore').write_text('''__pycache__/
*.py[cod]
.venv/
.deps/
.tools/
.build/
build/
*.kicad_prl
*.lck
*-backups/
.DS_Store
publication/
research/*.pdf
research/*.html
research/jlc-review/*.html
research/jlc-review/*-jlc.json
''')

(DEST / 'THIRD_PARTY_NOTICES.md').write_text('''# 部品ライブラリ・資料の出典

基板にはKiCad標準ライブラリを基にしたフットプリントと、本プロジェクト用の端子対応・配置を使用しています。
KiCad由来のライブラリに関する条件は[KiCadライブラリの案内](https://www.kicad.org/libraries/license/)を参照してください。

LCSC/EasyEDAの部品図は、実装座標の照合に使用した技術資料です。
取得元は `outputs/assembly-review/Supplier_Pad_References.json` と部品番号のリンクに記録しています。

ファームウェアのPico SDK・TinyUSB・newlib・GCCランタイムに関する出典と配布条件は、`firmware/DEPENDENCIES.md`と`firmware/LICENSES/`を参照してください。
''')

readme = (ROOT / 'README.md').read_text()
(DEST / 'README.md').write_text(readme)

# Every package checksum refers to the curated public files, not private copies.
package = DEST / 'outputs/jlc-one'
included = [package / 'README.txt'] + sorted(path for folder in ['upload', 'reference', 'checks']
                                           for path in (package / folder).rglob('*') if path.is_file())
(package / 'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(package)}\n' for path in included))
included.append(package / 'SHA256SUMS.txt')
with zipfile.ZipFile(DEST / 'outputs/Chiptune_JLC_ONE_Public.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for path in included:
        archive.write(path, 'Chiptune_JLC_ONE/' + str(path.relative_to(package)))
with zipfile.ZipFile(DEST / 'outputs/Chiptune_JLC_ONE_Public.zip') as archive:
    assert archive.testzip() is None
print(DEST)
print('Files:', sum(path.is_file() for path in DEST.rglob('*')))
