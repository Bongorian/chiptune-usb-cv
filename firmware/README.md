# Chiptune USB/CV v0.10.0-alpha

SAA1099 × 1、SN76489AN × 2、RP2040の専用基板用です。単一の発音エンジン、構成可能な音色、DIRECT/POLY、独立保存とWave Labを備えます。実機検証の対象と手順は[BRINGUP.md](BRINGUP.md)に記載しています。

[UF2](dist/chiptune-v0.10.0-alpha.uf2) / [検証結果](dist/Verification.json) / [実機確認](BRINGUP.md) / [音量・音程の根拠](LEVELS_AND_PITCH.md)

**保存形式はversion 10、SysExはprotocol 2 / schema 10です。旧形式は移行せず無視します。旧USER音色、CV校正、明るさは引き継ぎません。起動時は消去・書き込みを行わず、初期値をRAMへ作ります。既存設定を残す必要がある場合は、新版から保存する前に退避してください。**

## 最初に弾く

初期ProjectはMIDI 10。USBとTRSの同じMIDI chが同じTrackを操作しますが、Note OffとSustainの所有権は入力元ごとに区別します。

| MIDI ch | Track | 音源 | 初期動作 |
|---|---|---|---|
| 1–6 | 1–6 | SAA 1–6 | 各1音、ステレオ |
| 7–9 | 7–9 | SN L/Rの3トーン | 各1音、同じノートを左右へ並列出力 |
| 10 | 10 | SN L/Rのノイズ | 1音、左右へ並列出力 |
| 11–16 | — | 未割り当て | Advancedで変更可能 |

Track 11–14は初期状態OFF。14は物理回路の総数で、初期配置の音数は10です。PAN中央では両側を同じ音量にし、Track PANとCC10で左右へ振ります。SNの初期デチューンは0。初期試聴はnote 45=A2=110 Hz、基準はnote 69=A4=440 Hzです。

特殊音色専用の固定MIDI chはありません。音色を読み込んだTrackの受信chで演奏します。たとえばch10にTone 3連動ノイズ／Metalを載せる場合はch9の回路と競合するので、そのままでは拒否します。ch9をOFFにして空けるか、POLY FOCUSや配置テンプレートを明示的に選びます。

## 3ボタン操作

| 操作 | 動作 |
|---|---|
| Homeで− / ＋ | Track選択 |
| Homeで短いOK | PLAY / SOUND |
| HomeでOK長押し | Menu |
| 一覧で− / ＋、OK | 項目選択、編集へ入る |
| 編集で− / ＋、OK | 値変更、確定 |
| OK長押し | 編集取消、または戻る |
| − / ＋長押し | 数値の連続変更・加速 |
| −と＋を約0.9秒保持 | PANIC |

PLAY / SOUNDには音色選択、音量、PAN、ミュート、Track ON、モード、声数、ADSR、主要音色パラメータ、受信ch、A2試聴、保存があります。音色はFACTORYで候補を選び、LOAD PATCHで適用します。候補選択だけでは発音を変更しません。

Advancedには音源構造、ENV、ノイズ、レイヤー、移調、LFO、モーフィングを置きます。CLOCK、SEQUENCE/ARP、MOD、CV、SYSTEMはMenuから開きます。SYSTEMの見出しはV0.10です。未保存を示す「*」は操作後の目印であり、個々の保存対象の差分判定ではありません。

競合時は元の構成・別Trackの発音を維持し、理由と代替案を表示します。POLY FOCUSとDIRECT10 RESET、保存・読込・USB更新は本体上で確認画面を出します。

## DIRECT / POLY / LEGATO

- DIRECT: Trackごとに1音。後着ノートを優先し、離鍵時は保持中の前のノートへ戻ります。
- LEGATO: 単音の復帰・乗り換えでソフトエンベロープを継続します。
- POLY: Trackに予約した1–6声で演奏し、そのTrack内だけで古い声／リリース中の声をスティールします。

**POLY FOCUS**は同じチップのほかのTrackをOFFにし、選択Trackへ可能な最大声数を集める明示的操作です。別チップの演奏は保持します。SAA通常Toneなら6声、左右並列SN Toneなら3声。ENVやレイヤーでは音色が必要とする回路数に従って減ります。単にPOLYへ変えて声数を増やす操作は、ほかのTrackの予約回路を勝手に奪いません。

Wave LabにはMIDI10、SAA 6声POLY、SAA Tone＋ENVの配置テンプレートもあります。テンプレートとProject読込は全体の演奏を停止して配置を置き換えます。

## 音色の能力と制約

| 音源 | 実装した発音 |
|---|---|
| SAA | Tone、Noise、Tone+Noise、8形状ENVの発振／AM、8/16段、R反転、ステレオ振幅 |
| SAA ENV layer | ENV出力＋独立した補助Tone/Noise/Mix、音程差・レベル・減衰 |
| SN | 1–3 Toneの積層、White/Periodic Noise、固定3レート、Tone 3連動、Tone+Noise |
| SN stereo | 左右並列、左右単独、PAN、左右デチューン |
| 共通 | ADSR、独立ピッチエンベロープ、LFO、トレモロ、グライド、4マクロ、8変調ルート、同構造音色間モーフ |

SAA ENVは出力3/6とクロック2/5を予約します。補助レイヤーは同群の1/4を追加します。SAAノイズ速度は各3声の群で共有し、Key Trackには1/4のトーン発振回路が必要です。異なるノイズ速度を同じ群へ混在できません。通常Tone+NoiseのKey Trackはキャリアとノイズクロックを分離して予約します。ENV補助レイヤー自身をKey TrackクロックにしたTone+Noiseでは、ノイズ音程・スイープがその補助トーンにも影響します。

SN Tone 3連動ノイズは各側のTone 3を専有します。MetalはそのTone 3も聞こえる構造で、ToneとNoiseの音程を独立には設定できません。White NoiseのKey Trackは帯域制御で、楽音の基音を保証しません。左右のSNは独立音源ですが、基板の外部クロックは共通です。クロック変更は両SNのすべてのトーン・ノイズへ作用します。

ENV oscillatorのZero/Maximum形状はDC／一定振幅であり、周期波形ではありません。単発形はハードウェアENVのワンショットです。通常トーンのPWM、フィルター、サンプル再生はありません。音色構造を変えるモーフは行わず、同じチップ・Source・ENV形状・Layers等の間で連続値を補間します。利用不能なパラメータはWebでは隠します。

[音作り](SOUND_DESIGN.md) / [クロック](SN_CLOCK.md) / [設計と性能評価](ARCHITECTURE.md)

## MIDI、Sequence、CV

USB MIDI 1.0、TRS Type A 31,250 bps、Pitch Bend（RPN 0で幅設定）、CC1/7/10/11/64/120/121/123、チャンネル／ポリプレッシャー（Track変調源）、Program Changeに対応します。Program Change 0–15はUSER Patch 1–16です。CC16–19はTrackの4マクロ（−100～100）。CC20–27は選択したClock inputからの共通SNクロック操作です。詳細は[SN_CLOCK.md](SN_CLOCK.md)。任意のUSB/TRS ch→Trackマスクでレイヤーやスプリットを構成できます。

16ステップSequenceとArpeggioは発音エンジンから分離したノート生成器です。MIDI Start/Continue/Stop、内部30–240 BPM、USB/TRS Clock、Gate Clockを扱います。Gate Clockにする入力1/2も選択できます。SequenceのmotionはMacro 1の宛先を動かします。

CV/Gateは2系統。1V/oct、8種類のスケール、0V基準ノート、ヒステリシス、グライド、変調、GateのNote/Retrigger/Step/Clock FXを扱います。0Vは有効な音程です。ADCが無効・古い場合はCV声と変調を解除します。PANIC後、保持されたGate Highでは再発音せず、Low→Highで復帰します。SYSTEMのZERO→1V校正後、Systemを保存します。

同じTrackをMIDI、CV、Sequenceへ重複割り当てると、そのTrackの声数内で競合します。独立演奏には別Trackと物理回路を割り当ててください。PANICはすべての入力、保持鍵、サステイン、試聴、発音を解除します。

## 保存単位

| 保存対象 | 保存内容 | 保存しない内容 |
|---|---|---|
| Patch ×16 | 音色パラメータと名前 | PAN、受信ch、声数、Sequence、System |
| Track ×16 | Patch、名前、演奏モード、音量、PAN、Macro、Morph | MIDIルート、Sequence、Project共通値、System |
| Project ×4 | 全Track、ルート、Sequence、CV/Gate、変調、SNクロック、ミックス、テンポ | USER Patch/Trackライブラリ、System |
| System | CV校正、明るさ | 音色、Track、Project |

音色変更とWebの「本体に反映」はRAM編集です。永続化は保存を選んだときだけです。保存済みProjectから参照されるMorph先を異なる構造へ上書きする場合は拒否します。先に参照を解除してProjectを保存してください。

保存は末尾64 KiB内の32 KiB×2バンクへ、連番・CRC・最後のコミットマーカーで行います。論理的な独立保存を1つの原子的なSettingsスナップショットへまとめます。書込失敗時はRAMの保存済みモデルもロールバックします。明示的保存中は発音を停止し、MIDIをフラッシュ書込後に再同期します。演奏中の自動保存はありません。物理的な電源断・保持寿命は実機未検証です。

## Wave Lab

[エディタの説明](../editor/README.md)。[公開版Wave Lab](https://bongorian.github.io/chiptune-usb-cv/)または`editor/`を配信するローカルHTTPサーバーを使用します。

```sh
python3 -m http.server 8766 --directory editor
# http://localhost:8766
```

Chrome等のWeb MIDI対応ブラウザーでUSB MIDIを接続し、入出力を選択します。protocol 2/schema 10/firmware 0.10.0の完全一致を確認してから書込を有効にします。未接続でも編集・v0.10 Track JSONの入出力ができます。双方向同期は明示的な読出しと書込応答によります。本体操作を自動で常時取り込む購読型同期はありません。

## ビルドと検証

```sh
cmake -S firmware -B build/tests -DCHIPTUNE_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests -j4
ctest --test-dir build/tests --output-on-failure
python3 firmware/tools/generate_editor.py
python3 firmware/tools/generate_lut.py
build/tests/editor_export > editor/factory.json
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" node --test editor/tests/*.test.js

bash firmware/tools/bootstrap_sdk.sh .deps/pico-sdk
cmake -S firmware -B build/firmware -DPICO_SDK_PATH="$PWD/.deps/pico-sdk" -DCMAKE_BUILD_TYPE=Release
cmake --build build/firmware -j4
python3 firmware/tests/check_pio_clock.py build/firmware/sn_clock.pio.h
python3 -m pip install -r firmware/tests/requirements-arm.txt
python3 firmware/tests/check_arm_boot.py build/firmware/chiptune.elf --output build/firmware/Boot_Verification.json
build/tests/performance build/worst-project.bin build/worst-settings.bin > build/Performance.json
python3 firmware/tests/check_arm_runtime.py build/firmware/chiptune.elf build/worst-project.bin build/worst-settings.bin --output build/firmware/Runtime_Verification.json
python3 firmware/tools/verify_build.py build/firmware --output build/firmware/Verification.json
```

ARM GCCをPATHへ置いてください。[依存関係](DEPENDENCIES.md)にSDKの固定版とライセンスがあります。ブラウザーテストにはPlaywrightとChrome/Chromium、localhostのサーバーが必要です。

```sh
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" EDITOR_URL=http://127.0.0.1:8766 CHROME_BIN=/path/to/chrome node editor/tests/browser.cjs
```

配布UF2はRP2040 family ID、BINとの一致、全ブロック、USB MIDI記述子、2 MiB容量、設定領域の非重複を検査します。開発用USB VID/PIDのため、製品として配布する際は割り当てを確認してください。ホスト試験とARM命令実行は実機動作の証明ではありません。実時間の最悪値は診断の`tick_max_us`と`tick_overruns`を基板で測定してください。
