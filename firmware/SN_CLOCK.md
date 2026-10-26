# SN CLOCK v0.10

Menu → SN CLOCK: ALL、またはWave LabのSN共通クロックを開きます。基板上でSN左右は共通の外部クロック（GP16）を使います。初期MIDI ch7–10を含む全SN音源へ作用します。左右別のクロック設定はできません。SAAの8 MHzは独立です。

REFERENCE=4/2/1 MHz（初期2 MHz）で通常音程の分周器を計算します。FXは実クロックを250 kHz～4 MHzの範囲で変えるため、音程・ノイズ速度が一緒に変わります。音程の自動補正はしません。REFERENCE変更時はSNを消音します。

| FX | 動作 |
|---|---|
| OFF | REFERENCE固定、BASE/DEPTH無視 |
| MANUAL | BASEの半音低下量を保持 |
| RISE | トリガー後、低いクロックからBASEへ上昇 |
| TRIANGLE | BASEからBASE+DEPTHを往復 |
| SQUARE | BASE/BASE+DEPTHを交互に切替 |
| RANDOM | 1周期ごとに低下量を選択して保持 |
| SEQUENCE | 編集可能な8段階のDEPTH比を巡回 |
| DIVE | トリガー後BASEからBASE+DEPTHへ下降 |

BASE/DEPTHは0–48半音。REFERENCEを含めた総低下量を4 MHzから48半音（250 kHz）までに制限します。2 MHz基準ならBASE12=1 MHz、24=500 kHz、36=250 kHzです。

RATEは0.25/0.5/1/2/4/8/16/32/50/80/100/160/250/400/600/800 Hz。SYNCはFREE、1/2/3/4/6/8/12/24 MIDI clocks/cycle。RISE/DIVEはトリガーとRATEで動きます。RETRIGGERとTRIGGER FROMで再トリガー元を選びます（Any SN、MIDI SN、CV1、CV2、ch10、Tone、Noise、Melody/CV）。再トリガーは既に鳴っているSNへも作用します。

指定したClock input（USBまたはTRS）からのCC:

| CC | 内容 |
|---|---|
| 20 | FX番号0–7（それ以上は7） |
| 21 / 22 | BASE / DEPTH、0–127→0–48半音 |
| 23 | RATE、0–127→16段階 |
| 24 | SYNC、0–127→9段階 |
| 25 | RETRIGGER、64以上=ON |
| 26 | 64以上で手動トリガー |
| 27 | REFERENCE番号0=4 MHz、1=2 MHz、2以上=1 MHz |

内部はPIOと250 µs整数処理で生成します。レジスター書込はクロックに応じた最大136 µsの/WE LOWを非同期で保持します。高速変調は最新値へまとめるので、低クロックで全更新をICへ逐次送る保証はありません。PIO命令とソフトの待機は自動テスト済みですが、周波数・波形・/WE幅と音は実機未確認です。
