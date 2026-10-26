# Chiptune USB/CV

SAA1099 × 1、SN76489AN × 2、RP2040を搭載したPSG音源のハードウェア・ファームウェアプロジェクトです。
USB MIDI、TRS MIDI、2系統のCV／ゲート入力と、OLED・3ボタンによる操作を想定しています。
回路図、配線済みPCB、JLCPCB向け製造データ、設計資料を収録しています。

ファームウェアはv0.10.0-alphaです。初期配置はMIDI 10（ch1–6: SAA、ch7–9: 左右SNトーン、ch10: 左右SNノイズ）。DIRECT/POLY演奏、PAN/CC10、音色編集、Patch/Track/Project/System保存に対応します。[操作資料](firmware/README.md)と[音源構成](firmware/SOUND_DESIGN.md)を参照してください。

保存形式はversion 10、Web MIDI通信はprotocol 2/schema 10です。旧形式の音色・CV校正・明るさは移行しません。起動時のフラッシュ消去・書き込みは行いません。

自動テスト・ARMビルド・UF2検証の結果は[検証記録](firmware/dist/Test_Verification.json)に記録しています。実機の音声出力、電気特性、1 ms更新の最悪実行時間は未検証です。[検証範囲](firmware/ARCHITECTURE.md)と[実機確認項目](firmware/BRINGUP.md)を参照してください。

![PCB配線図](outputs/review/routing-traces-all.png)

[**Wave Lab — 音色・波形エディタを開く**](https://bongorian.github.io/chiptune-usb-cv/)

Wave Labのソースは`editor/`にあります。公開版のほか、ローカルHTTPサーバーでも実行できます。[音作り](firmware/SOUND_DESIGN.md)と[エディタの使い方](editor/README.md)を参照してください。

## 仕様

| 項目 | 内容 |
|---|---|
| 基板 | 85 × 54 mm、4層、FR-4 1.6 mm、角R3、M2用取り付け穴 |
| マイコン | RP2040、外部QSPIフラッシュ |
| 音源 | SAA1099 DIP18 × 1、SN76489AN DIP16 × 2 |
| 電源 | USB-C給電 |
| MIDI | USB MIDIデバイス、3.5 mm TRS Type A入力 |
| CV | 1 V/oct入力 × 2、系統別の2点校正を想定 |
| ゲート | 0–5 V／0–10 V入力 × 2 |
| オーディオ | 3.5 mmステレオライン出力、入力インピーダンス10 kΩ以上の外部アンプ向け |
| 操作 | 0.96インチ128 × 64 OLED、3ボタン |

## リポジトリ構成

| ディレクトリ | 内容 |
|---|---|
| `routed/` | 配線済みKiCadプロジェクト、シンボル、フットプリント |
| `hardware/` | 回路・配置生成用の未配線プロジェクト |
| `editor/` | GitHub Pages向けWeb MIDI音色エディタ、波形・ADSR・16ステップ・クロック・CV設定 |
| `firmware/` | RP2040音源エンジン、USB／TRS MIDI、CV／ゲート、OLED UI、プリセット、UF2 |
| `mechanical/` | 3Dプリント用ケース、STL・STEP、寸法図・組み立て手順 |
| `docs/` | 回路仕様、操作設計、配線ルール |
| `scripts/` | 設計生成、配置、配線、検証、製造データ出力用スクリプト |
| `outputs/jlc-one/` | JLCPCB向け提出ファイルと組み立て資料 |
| `outputs/assembly-review/` | 実装座標・ピン1・極性の照合資料 |
| `outputs/review/` | 基板図とUIモックアップ |
| `research/` | 部品カタログ属性と実装ライブラリ参照図 |
| `variants/` | 設計比較用の基板データ |

## KiCadで開く

[`routed/Chiptune_USB_CV.kicad_pro`](routed/Chiptune_USB_CV.kicad_pro) を開いてください。
製造用のPCBは同じディレクトリの `Chiptune_USB_CV.kicad_pcb` です。

KiCad 9.0.9で作成しています。シンボルとフットプリントは同梱し、`${KIPRJMOD}`から参照します。
一部の3DモデルにはKiCad標準ライブラリが必要です。

## 製造データ

[`outputs/jlc-one/upload/`](outputs/jlc-one/upload/) にJLCPCB／JLCONE向けの提出ファイルがあります。

| ファイル | 用途 |
|---|---|
| [`Chiptune_Gerber.zip`](outputs/jlc-one/upload/Chiptune_Gerber.zip) | 基板製造データ |
| [`Chiptune_BOM.csv`](outputs/jlc-one/upload/Chiptune_BOM.csv) | 部品表・LCSC部品番号 |
| [`Chiptune_CPL.csv`](outputs/jlc-one/upload/Chiptune_CPL.csv) | 表面実装座標・回転角 |

Gerber ZIP、BOM、CPLをそれぞれ対応する欄へ指定します。
[製造資料一式のZIP](outputs/Chiptune_JLC_ONE_Public.zip) は展開し、内部の提出ファイルを使用してください。

表面実装対象は141点、BOMは41行・40品種です。
音源ICとDIPソケット、OLEDモジュール、任意のSWDヘッダーは手実装します。
組み立て手順と製造条件は [`outputs/jlc-one/README.txt`](outputs/jlc-one/README.txt) を参照してください。
発注時は部品在庫と実装プレビューのピン1・極性・向きを確認してください。

## 3Dプリント用ケース

[ケースの設計・印刷・組み立て資料](mechanical/README.md)にSTL、STEP、寸法図を収録しています。
印刷用データは[ケースZIP](mechanical/exports/Chiptune_Enclosure_3DPrint.zip)から取得できます。
上下2分割、底面のM2インサート固定、音源ICとOLEDの開口、3ボタンの押し子を備えます。
ケース本体は89.8 × 58.8 × 23.0 mmです。OLEDは別モジュールを基板上の5 mmスペーサーで取り付けます。

## ファームウェア

[ファームウェアの操作資料](firmware/README.md)と[配布UF2](firmware/dist/chiptune-v0.10.0-alpha.uf2)を収録しています。
本体の3ボタンで、SAAのENV発振・ソフトADSR、SNのステレオ音色・実クロック変調、MIDI/CV/Gateの割り当て、パターン編集と保存を操作できます。
[Androidからの更新手順](firmware/ANDROID_UPDATE.md)、[実機確認項目](firmware/BRINGUP.md)、[検証記録](firmware/dist/Verification.json)も参照してください。
−と＋を押しながらUSBを接続するか、SYSTEM > USB UPDATEからROMブートローダーへ入れます。

## 開発状況

- 基板検査: DRC 0、未接続0、回路図との一致問題0、ERC 0。
- 設計照合: 152フットプリントの配置・端子接続と、音源バスの768ケースを検査済み。
- ファームウェア: v0.10.0-alpha。Track/Patch/Voice/物理回路を分離した単一エンジン。DIRECT/POLY、SAA ENV/AMとSNレイヤー、独立Patch/Track/Project/System保存、Wave Lab protocol 2。ホスト・ARM・Web試験とUF2検証は[検証記録](firmware/dist/Test_Verification.json)を参照。実機評価は未完了。
- 実機検証: 電源電流・突入電流、音声出力、CV校正、取り付け公差の評価は未完了。

[OLEDの描画例](firmware/assets/ui-v10.png)は、実装したCの描画処理から生成しています。

## 検証コマンド

Ubuntu上のKiCad CLIと、`pcbnew`を利用できるPython環境を使用します。
画像出力にはCairoSVGが必要です。

```bash
kicad-cli pcb drc --format json --schematic-parity --exit-code-violations \
  -o outputs/drc-routed.json routed/Chiptune_USB_CV.kicad_pcb
kicad-cli sch erc --format json --exit-code-violations \
  -o outputs/erc-routing.json routed/Chiptune_USB_CV.kicad_sch
/usr/bin/python3 scripts/check_routing.py
/usr/bin/python3 scripts/check_bus_encoding.py
```

検証レポートは `outputs/`、製造パッケージの検査結果は `outputs/jlc-one/checks/` にあります。
配置・配線の再生成スクリプトはKiCadファイルを書き換えるため、設計変更用のブランチで実行してください。

## 設計資料

- [基板説明資料（日本語・9ページPDF）](output/pdf/Chiptune_USB_CV_Guide_JA.pdf)
- [回路仕様・操作設計](docs/USB-CV-design-notes.txt)
- [配線方針・線幅](docs/routing-notes.txt)
- [実装ライブラリの照合記録](outputs/assembly-review/Assembly_Review.txt)
- [ピン1・極性の参照図](outputs/assembly-review/Placement_Pin1.png)
- [部品ライブラリ・資料の出典](THIRD_PARTY_NOTICES.md)
