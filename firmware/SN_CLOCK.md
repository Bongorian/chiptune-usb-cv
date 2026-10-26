# SN CLOCK v0.11

Menu → SN CLOCK、またはWave LabのSN共通クロックを開きます。SN76489AN左右はGP16の外部クロックを共有します。SAA1099の8 MHzは独立しており変更しません。

## BASE CLOCKとPIO

初期値は2,000,000 Hz。REFERENCEの1/2/4 MHz選択は廃止しました。通常はHzで指定し、AdvancedではPIOのXとdivider×256を指定します。Hzを編集するとHzモードへ、PIO値を編集するとDirect PIOへ切り替わります。本体のHz編集は整数単位です。小数HzはWebのHz入力、または本体のDirect PIOで設定できます。

使用する9命令のPIOプログラムは、片側周期が`X + 5` SMサイクルです。Xは0〜4,294,967,295、16.8分周値Dは256〜16,777,216（256倍表記）。実際に設定した値から次式で周波数を計算します。

```
clock_hz = 128000000 × 256 / (2 × (X + 5) × D)
```

生成範囲は約0.0000002273736752 Hz〜12,800,000 Hzです。下限・上限はこのPIO実装から算出します。250 kHz／4 MHzでのソフトウェアクランプはありません。範囲内は量子化された最近傍の候補を使います。4 MHz超はTIの仕様を超えるためEXPERIMENTALを付けますが、使用を禁止しません。この範囲はGPIO、レベル変換、実ICで測定した動作保証範囲ではありません。

同じXでは分周値を更新してSM分周器を再始動します。Xの変更ではSMをLOWから再始動します。変更境界の位相連続性は保証せず、短縮パルスやグリッチが生じ得ます。表示する周波数は定常時の設定値からの計算であり、変更過渡を含めた実測値ではありません。

## PITCH FOLLOW

| モード | BASE CLOCK変更 | FX変調 |
|---|---|---|
| RAW | 補正しない（固定2 MHz基準のノート分周値） | 補正しない |
| BASE（初期値） | 設定済みPIOの基本周波数で補正 | 補正しない |
| FULL | 補正する | 現在の設定済みPIO周波数で補正を試みる |

トーン分周値は`round(clock_hz / (32 × target_hz))`。1〜1023へ個別にクリップし、clipマスクへ記録します。共通クロックを拒否したり、発音中の全SNノートを停止したりしません。左右の全アクティブトーン、レイヤー、PULSE／Tone 3連動Noiseのクロック用Tone 3にも適用します。PULSEのTone 3目標は楽音周波数の15倍です。固定3レートNoiseのスペクトル変化は補正しません。

FULLの目標分周値更新は音源の1 ms周期、FXは協調ループ内の250 µs目標周期です。レジスタ転送が遅ければ追従も遅れます。Advancedのdesired/sent、target Hz、sent error cents、clip、待ち時間で区別できます。実クロックの停止中もノート所有権は保持します。

## MOD A / MOD B

各スロットはOFFが初期値です。OFFは変調量0、MANUALはOFFSETのみ。その他はOFFSETを中心とし、符号付きDEPTHを掛けた変調量を出力します。RISE/DIVE/CURVEは−1〜+1のワンショット、周期波形は概ね−1〜+1です。負のDEPTHで向きを反転できます。

| DOMAIN | OFFSET / DEPTHの単位と計算 |
|---|---|
| MUSIC | セント。`Hz × 2^(amount / 1200)` |
| PHYSICAL Hz | Hz。`Hz + amount` |
| PHYSICAL ratio | ppm。`Hz × (1 + amount / 1000000)` |
| PHYSICAL PIO period | 基本PIO分周値での半周期サイクル数を加算して逆数変換 |

MOD A→MOD Bの順に合成して最終周波数をPIOへ量子化します。異なるDOMAINの順序は結果に影響します。生成不能な結果はUNAVAILABLEとし、進行中の転送を最後の有効クロックで完了してから停止します。有効値へ戻ると再開し、最新レジスタへ収束します。

FX番号順はOFF、MANUAL、RISE、TRIANGLE、SQUARE、RANDOM、SEQUENCE、DIVE、SINE、SAW、RANDOM WALK、SLEW RANDOM、WOW、FLUTTER、CURVE ENVELOPE、RATCHET、BURST、AUDIO-RATE、SAW DOWNです。

- RANDOMは周期ごとの保持値、RANDOM WALKはその1/4幅の積み重ね、SLEW RANDOMは保持値へ連続的に近づく変調です。
- WOWは主正弦と7倍成分、FLUTTERは主正弦と3倍成分を混合します。
- CURVE ENVELOPEはLinear／Squared／Smooth。RATCHETは1周期中8パルス、BURSTはその1周期分です。
- AUDIO-RATEは正弦変調で、Webで選択すると低いRATEを250 Hzへ変更します。最大4 kHz目標の更新に従い、Nyquistを超えた設定や処理遅延では折り返し・グリッチが生じます。音声帯域の全RATEを忠実に再現する保証ではありません。

RATEは0.001 Hz単位、0は位相停止です。WebではHzと設定テンポでの秒・拍を表示します。SYNCはFreeまたは1/2/3/4/6/8/12/24 MIDI clocks/cycle。MIDI Clockの受信間隔から位相を補間し、次の到着で再同期します。Start等によるカウンタの変化も再基準化します。Clock途絶時は最後の間隔で予測を続けます。

RETRIGGERはスロットごとに設定します。TRIGGER FROMは両スロット共通でAny SN、MIDI SN、CV1、CV2、ch10、Tone、Noise、Melody/CV。CV/GateルートのClock FX、CC26、本体TRIGGER、Web手動トリガーは再トリガーできます。手動操作は両スロットを再始動します。

## 転送と状態

現行の`routed/design.json`ではU11/U12のREADY（pin 4）は未接続、RP2040 GP23/24も未接続です。古い`scripts/design.py`のREADY割当は現行配線の根拠には使いません。

SN /WEと共有データ・CSを非同期で保持し、TI資料の約32クロックに対して40クロックの保守的な完了予算を使います。PIO設定と経過時間から完了クロック数を下方推定し、設定変更時は不明な端数周期を捨てます。固定16〜136 µsの制限・タイムアウトはありません。分周器の2 bytesは同じスナップショットで完了します。新しい要求は最新値へまとめ、PANICの消音を優先します。

Stop操作は進行中の2-byte転送まで完了してからLOWで停止します。停止中に新規SN転送を始めません。低速でStop待ちが長い場合はStopを解除し、BASE CLOCKを上げれば完了を早められます。低速だけを理由に強制ミュートしません。

**共有データバスを保持している間はSAAのレジスタ転送も待ちます。** SAAの8 MHz発振は継続します。USB/TRS MIDI、CV、Gate、OLED、ノート計算は処理を続けます。極低速や頻繁なタイミング変更では転送予算の下方推定により実際以上に待つ場合があります。これを更新遅延として表示し、高速へ戻すと最新値へ収束させます。

| 状態 | 意味 |
|---|---|
| VALID | 生成可能、検出されたクリップ・転送遅延なし |
| DEGRADED | 生成可能だが実験領域、音程クリップ、1 ms超の転送待ち、FULL追従待ち等がある |
| UNAVAILABLE | FX結果等をPIOで生成できない。進行中の転送後に停止 |
| STOPPED | 明示Stopが完了 |

VALIDは実機での動作保証を意味しません。Webのグラフは設定から計算した4秒プレビューです。実測器ではありません。

## MIDI CC（選択したClock input）

| CC | 動作（MOD A） |
|---|---|
| 20 | FX番号0〜18（超過は18） |
| 21 | OFFSET = `(value − 64) × 100`（現在のDOMAINの単位） |
| 22 | DEPTH = `value × 100`（現在のDOMAINの単位） |
| 23 | RATE = `0.25 + value × 0.25` Hz |
| 24 | SYNC、0〜127→0〜8 |
| 25 | RETRIGGER、64以上ON |
| 26 | 64以上で手動トリガー |
| 27 | FOLLOW、0 RAW／1 BASE／2以上FULL |

## 検証と資料

ホスト試験、リンク済みARMコードでの起動・停止・復帰・PANIC試験、PIO命令／16.8分周の周期検証、UF2領域検証を行います。詳細は`dist/*Verification.json`。1 ms音源と250 µs FXの実機WCET、GPIO過渡、レベル変換、SN個体差、保証外クロック、音程・ノイズ音色、SAAを含む共有バスの実効遅延は未測定です。

- [RP2040 datasheet, PIO clock divider / instruction timing](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Texas Instruments SN76489AN datasheet（TI資料のミラー）](https://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf)
- [実機確認手順](BRINGUP.md)
