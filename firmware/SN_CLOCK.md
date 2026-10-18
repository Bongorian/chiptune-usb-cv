# SN CLOCK：実クロックを使う音作り

v0.6.1-alphaは、GP16からSN76489ANのCLOCK端子へ送る信号を直接変調します。
トーン用分周器をREFERENCEの4／2／1 MHzで設定したまま実クロックを変えるため、音程とノイズ速度が一緒に変化します。
音程を保つための自動補正は行いません。SAA1099の8 MHzクロックは独立して維持します。

**SN1／SN2は基板上でクロックを共有しています。** 左右の全SNトーン・ノイズ、CVで発音しているSNの声にも同じ変調がかかります。
SNの新しいノートがクロックを再トリガーすると、すでに鳴っているSNの声も変化します。
通常の指定音程を保ちたい場合はMODEをOFFにしてください。新規設定のREFERENCEは2 MHzです。

## 操作

ホームで**MOTION**を開き、**SN CLOCK: BOTH**を選びます。操作全体は[README](README.md)を参照してください。
−／＋で項目を選択、OKで編集、−／＋で変更、OKで確定します。発音中に変更できます。
クロック図は設定から描く模式図です。端子の実測波形ではありません。

| 項目 | 内容 |
|---|---|
| MODE | OFF、MANUAL、RISE、TRIANGLE、SQUARE、RANDOM、SEQUENCE、DIVE |
| BASE DROP | REFERENCEから下げる半音数。0–48。2 MHz基準なら12で1 MHz、24で500 kHz、36で250 kHz。下限250 kHz |
| DEPTH | BASEからさらに下げる幅。0–48半音。合計は48半音で制限 |
| RATE | 0.25、0.5、1、2、4、8、16、32、50、80、100、160、250、400、600、800 Hz |
| MIDI CLOCKS/CYCLE | FREE、1、2、3、4、6、8、12、24クロックで1周期 |
| NOTE RETRIGGER | ONでSNのNote On／CVゲート立ち上がり時に位相とスイープを再開始 |
| CLOCK/CC SOURCE | USB／TRS。ARPEGGIOのCLOCK USB/TRSと同じ設定 |
| TRIGGER | OKで手動再トリガー。NOTE RETRIGGERがOFFでも実行 |
| REFERENCE | 4／2／1 MHz。生成と音程計算の基準。変更時は消音 |
| PAIR WIDTH | 左右SNペアの合計ピッチ差、0–30 cents |
| TRIGGER FROM | ANY SN／MIDI SN／CV1／CV2／DRUM CH10／SN TONES／SN NOISE／MELODY/CV |

| MODE | 動作 |
|---|---|
| OFF | REFERENCEのクロック固定。BASE／DEPTHを無視 |
| MANUAL | BASEだけでクロックを低下。全体を低音・遅いノイズにする |
| RISE | トリガー時にBASE＋DEPTHへ下げ、時間とともにBASEへ戻す。実クロックと音程は上昇 |
| TRIANGLE | BASEからBASE＋DEPTHまで往復。大きな揺れや速い周波数変調 |
| SQUARE | BASEとBASE＋DEPTHを切り替える。2つの音程・ノイズ速度を交互にする |
| RANDOM | 1周期ごとに低下量をランダムに選び、その周期中は保持 |
| SEQUENCE | 1周期を8段階に分け、DEPTH比0、1/2、1、1/4、3/4、1、1/2、0を巡回 |
| DIVE | トリガー時はBASE、時間とともにBASE＋DEPTHへ落とす。大きなピッチ下降 |

RISE／DIVEの時間はRATEの逆数です。2 Hzなら約500 ms、100 Hzなら約10 msです。
この2モードはMIDI同期を使わず、トリガーとRATEで動きます。
NOTE RETRIGGERがOFFの場合はTRIGGERまたはCC26で開始してください。

TRIANGLE／SQUARE／RANDOM／SEQUENCEは、FREEでRATEを使い、同期ではARPEGGIOのCLOCK MODEが選んだ外部／内部Clockで進みます。
EXTERNALではClockが来ない／Stopした場合は現在の段階を保持します。AUTOはClock欠落時に内部へ戻り、明示的Stopは保持します。FREEではMIDI Stopに影響されません。
再トリガー後のMIDI周期は次の受信Clockから数え直します。

## まず試す設定

USB MIDIのTARGETをSN STEREO、SN1 LEFTまたはSN2 RIGHTにして演奏します。全体のMASTER音量は控えめから調整してください。

| 音の方向 | PATCH | MODE / BASE / DEPTH / RATE | その他 |
|---|---|---|---|
| 荒い周期ノイズ | PERIODIC（PC9） | RANDOM / 0 / 36 / 250 Hz | SYNC=FREE、RETRIGGER=OFF |
| 金属的な揺れ | PERIODIC（PC9） | TRIANGLE / 12 / 24 / 160 Hz | SYNC=FREE、RETRIGGER=OFF |
| 低く落ちる衝撃音 | KICK（PC10） | DIVE / 0 / 36 / 2 Hz | RETRIGGER=ON |
| 切り刻むノイズ | NOISE PAD（PC14） | SQUARE / 0 / 36 / 32 Hz | RETRIGGER=OFF |
| 拍に合う音程崩し | DRY SQUARE（PC15） | SEQUENCE / 0 / 24 | SYNC=4 CLOCKS/CYCLE、RETRIGGER=OFF |

音の表現は狙いで、実ICでの試聴は未実施です。16音色の定義に加わる全体エフェクトなので、組み合わせを変えられます。
[キャラクターと演奏設定](CHARACTER_GUIDE.md)に、プリセットとチャンネル／CV割り当てを記載しています。
v0.1の移行ではSN CLOCKをOFFにし、v0.2の移行では旧FXを維持します。どちらも4 MHz基準を維持します。
例を復元するにはPRESETでSLOT 5–8を選びINIT、必要ならSAVEします。INITは選択枠の設定を初期化します。

## DAWからの操作

CLOCK/CC SOURCEで選んだ入力と、その入力のチャンネルフィルターを通るCCを使います。
複数チャンネルの音を出していても、制御するクロックは2個のSN共通です。

| CC | 値 |
|---|---|
| 20 MODE | 0 OFF / 1 MANUAL / 2 RISE / 3 TRIANGLE / 4 SQUARE / 5 RANDOM / 6 SEQUENCE / 7 DIVE。8以上は7 |
| 21 BASE | 0–127を0–48半音へ変換 |
| 22 DEPTH | 0–127を0–48半音へ変換 |
| 23 RATE | 0–127を上記16段階へ変換。各段階の順序は表の左から |
| 24 SYNC | 0–127をFREE＋8種類のMIDI分割へ変換 |
| 25 RETRIGGER | 0–63 OFF / 64–127 ON |
| 26 TRIGGER | 64以上を送るたびに再開始 |
| 27 REFERENCE | 0＝4 MHz／1＝2 MHz／2以上＝1 MHz。変更時は全音源を消音 |
| 28 WIDTH | 0–127を合計0–30 centsへ変換 |
| 29 TRIGGER FROM | 0 ANY SN / 1 MIDI SN / 2 CV1 / 3 CV2 / 4 DRUM CH10 / 5 SN TONES / 6 SN NOISE / 7 MELODY/CV。8以上は7 |

例えばCC20=5、CC21=0、CC22=95、CC23=102、CC24=0、CC25=0で、おおむねDEPTH 36・RATE 250 HzのRANDOMになります。
これらのCCは他のMIDI機器と意味が一致する規格ではありません。
設定はRAMに反映し、PRESET SAVEで保存します。

## 実装と確認範囲

PIOが定常時50%デューティのクロックを生成します。各半周期は128 MHzの11–251回のループ＋5命令サイクル、16–256サイクルです。
250 kHz–4 MHzを整数周期241段階で生成し、低い側の境界で周期を切り替えます。FIFOが空なら直前の周期を維持します。
変調パラメーターは250 µsを目標にタイマー割り込みで更新し、ISR内は整数演算です。
長いIRQ停止後は再同期し、取り戻すための連続更新を抑えます。停止した時間の位相は早送りしません。
800 Hzでは1周期当たり5回の更新で、連続的なアナログFMとは異なる段階的な変化になります。
最高周波数側では周期の丸めが粗くなるため、半音指定と実際の変化量は厳密には一致しません。

SNは書き込みに約32入力クロックを必要とし、READYは本基板で未接続です。
書き込み待ちを設定中の最も遅いクロックから計算し、8 µsの余裕を加え、16–136 µsを確保します。
低速クロック時は継続するSNトーンの分周値更新を最大4 ms間隔にし、新しい発音は即時に設定します。
ADSR音量・SAA制御は通常の更新を続け、外部クロック変調は独立したタイマーで継続します。
多数の声を一度に更新すると処理時間は増えます。実機でMIDI負荷、CV応答、更新遅れとIRQジッターを測る必要があります。

起動時の消音処理はSN 4 MHzで行ってから設定中のエフェクトを適用します。
電源禁止／USBサスペンドでは変調タイマーとPIOを停止し、クロック端子をHi-Zにします。
フラッシュ保存中は発音を止め、IRQ停止中のPIOは直前のクロックを維持します。

PCテストは周期範囲、スイープ、ランダム、MIDI同期、再トリガー、CC、保存値、旧形式の移行を検査します。
pioasmの実機向け機械語を実行するモデルで、241周期の50%デューティ、756箇所の更新位相、FIFO空時の継続を検査しています。
基板端子の波形、ICの音、変調割り込みの実時間はまだ測定していません。

仕様参照：[TI SN76489AN pp.2–4,7](https://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf)、[RP2040 PIO](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)。


## v0.4の連携

CLOCK STEPSで8ステップの深度割合を編集・保存できます。SEQUENCEの基本速度はBASE DROP、最大変化幅はDEPTHで指定します。
CC30=0–7で編集ステップ、CC31=0–127でその深度割合0–100%を設定します。CLOCK/CC SOURCEの入力だけが有効です。
ARPEGGIOのCLOCK MODE=INTERNALとINTERNAL BPMを使えば、接続先からClockが来なくても同期パターンを動かせます。
CV ROLE=SN BASE DROP／SN FX DEPTHで実効値をCV操作できます。CVの変調入力のゲートは手動トリガーとして作用します。
詳細は[キャラクターと演奏設定](CHARACTER_GUIDE.md)を参照してください。
