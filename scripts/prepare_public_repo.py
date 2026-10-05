#!/usr/bin/python3
"""Build a curated public snapshot without personal attachments or web caches."""
import hashlib
import json
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


for folder in ['routed', 'hardware', 'firmware', 'scripts', 'docs']:
    for path in (ROOT / folder).rglob('*'):
        if path.is_file() and '__pycache__' not in path.parts and path.suffix not in {'.pyc', '.kicad_prl', '.lck'}:
            copy_file(path)
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
        cleaned = original.replace(str(ROOT) + '/', '').replace('[local attachment]/', '[local attachment]/')
        cleaned = cleaned.replace('[user screenshot]/', '[user screenshot]/')
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
メーカーのデータシート本文、注文表、ログイン情報、取得したWebページは含めていません。
''')

readme = '''# Chiptune USB/CV

SAA1099 × 1 と SN76489AN × 2 を使う、85 × 54 mmのチップチューン音源ガジェットの設計資料です。
現在の版はRP2040、USB-C給電／USB MIDIデバイス、TRS MIDI入力、CV入力2系統＋ゲート入力2系統です。

![配線](outputs/review/routing-traces-all.png)

## 現在の仕様

| 項目 | 内容 |
|---|---|
| PCB | 85 × 54 mm、角R3、角R中心のM2用穴、4層、FR-4 1.6 mm |
| MCU | RP2040、外部QSPIフラッシュ |
| 音源 | SAA1099 DIP18 × 1、SN76489AN DIP16 × 2、ソケットで手実装 |
| MIDI | USB MIDIデバイス入力、3.5 mm TRS Type A入力 |
| CV / gate | CV 1 V/oct × 2、ゲート0–5 V／0–10 V × 2 |
| 音声 | 3.5 mmステレオライン出力、外部アンプ等への接続 |
| UI | 0.96インチOLEDと3ボタン。画面は操作見本、実機FWは未完成 |
| 電源 | USB-C。電池・USBホスト・PCM5102は現在の版では使用しない |

## 開くファイル

- **配線済み原本:** [`routed/Chiptune_USB_CV.kicad_pro`](routed/Chiptune_USB_CV.kicad_pro)
- 配置・回路の元データ: `hardware/`（未配線。製造には使用しない）
- 設計仕様・操作: [`docs/USB-CV-design-notes.txt`](docs/USB-CV-design-notes.txt)
- 線幅・配線方針: [`docs/routing-notes.txt`](docs/routing-notes.txt)
- 部品／実装レビュー: [`outputs/assembly-review/Assembly_Review.txt`](outputs/assembly-review/Assembly_Review.txt)
- UI操作見本: [`outputs/review/chiptune-ui.html`](outputs/review/chiptune-ui.html)

KiCad 9.0.9で作成。シンボル・フットプリントはプロジェクトに同梱し、`${KIPRJMOD}`から参照します。
3Dモデルの一部はKiCad標準ライブラリのインストールが必要です。

## JLCPCB / JLCONEへの提出

[`outputs/jlc-one/upload/`](outputs/jlc-one/upload/) の次の3点を、それぞれ対応する欄に指定します。

1. `Chiptune_Gerber.zip` — このZIPを展開せず基板データとして提出
2. `Chiptune_BOM.csv` — メーカー型番・LCSC番号を含む部品表
3. `Chiptune_CPL.csv` — 表面の実装座標・部品別の回転／原点補正

[提出用まとめZIP](outputs/Chiptune_JLC_ONE_Public.zip) は保管・受け渡し用です。外側のZIPをそのまま基板欄に指定しないでください。
表面実装141点、BOM41行・40品種。OLED、音源IC／DIPソケット、任意のSWDヘッダーは手実装です。
J1のUSB-Cも調達・実装対象です。My Partsに使用可能在庫がある場合は利用でき、新規調達は同じC165948を選択します。

![ピン1と極性](outputs/assembly-review/Placement_Pin1.png)

ピンクの「1」は設計上の1番ピン、「K」はカソードです。ビュワー側の色付き点と同じ意味ではありません。
新しいプレビューの外形・リード位置は整合し、ユーザーは配置を確認済みです。
U1/U13/U16のピン1印は画像で一致を確認。U2/U3/U9/U14/U15は印を独立して判別できず、製造前に最終確認が必要です。
部品在庫・価格・実装対象は注文時に確認してください。

## 検証と残作業

KiCadのDRC 0、未接続0、回路図との一致問題0、ERC 0。
152フットプリントの座標・端子ネットを元データと照合し、音源バスの768ケースを検査しています。
レポートは `outputs/*verification.json`、`outputs/drc-routed.json`、`outputs/erc-routing.json` にあります。

実機ファームウェアは未完成です。`firmware/pinmap.h` と `sound_bus.h` はGPIO定義・音源書き込みヘルパーで、完成したUSB MIDIアプリではありません。
電源電流・突入、音声出力、CV校正、組み立て公差の実機検証は未完了です。
この公開スナップショットから発注・支払いは行っていません。

## 作業用スクリプト

Ubuntu上のKiCad CLIと `/usr/bin/python3` の `pcbnew` を使用します。

```bash
kicad-cli pcb drc --format json --schematic-parity --exit-code-violations \\
  -o outputs/drc-routed.json routed/Chiptune_USB_CV.kicad_pcb
kicad-cli sch erc --format json --exit-code-violations \\
  -o outputs/erc-routing.json routed/Chiptune_USB_CV.kicad_sch
/usr/bin/python3 scripts/check_routing.py
/usr/bin/python3 scripts/check_bus_encoding.py
/usr/bin/python3 scripts/package_jlc_one.py
```

PNGの生成にはCairoSVGを使用。その他の配置・ルーティングスクリプトは開発履歴として保存しています。
再生成・配線実験はコピーで行い、配線済み `routed/` を上書きしないでください。
部品カタログは再出力に必要な公開属性のみ保存し、WebページのキャッシュとメーカーPDFは除いています。

## 設計の経緯

- 当初は単4電池2本、USBホスト／デバイス切替を検討。
- USB給電・USB MIDIデバイス専用に変更し、OLED UIとCV／ゲート2系統を追加。
- 信号の流れに合わせて配置を整理し、四隅をR3・穴を角R中心へ統一。
- 信号別の線幅を定義し、4層で配線を完了。
- D2を赤0603 C2286、Q3/Q4をSOT-23 C20526のBasic品へ変更。
- 全実装ICとジャック・USBのライブラリ原点を照合し、CPLを補正。

注文表・個人のスクリーンショット・ブラウザキャッシュ・大量の途中配線結果は公開対象に含めていません。
部品図・KiCadライブラリの出典は [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) を参照してください。
'''
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
