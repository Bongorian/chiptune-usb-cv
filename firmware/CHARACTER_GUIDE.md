# 音源の特性と旧版の詳細設定資料

現行v0.7のMIDI配置は、ch1=SAA通常、ch2=SAA ENV/AM、ch3=SNステレオトーン、ch10=SNステレオノイズです。音色選択・MIDI番号・保存の仕様は[現行ファームウェア資料](README.md)を参照してください。以下には旧版の音色名・番号や操作名が含まれます。音源特性の参考資料として残しています。

> この資料はv0.4の旧ルーティング／音色番号を含む音源特性資料です。v0.5の操作と音色モデルは[README](README.md)を参照してください。

# 音源のキャラクターと演奏設定

v0.4.0-alphaの演奏用ガイドです。以下は各ICの機能を使う音作りの方針です。
実基板での試聴・音程・音量バランス・CV応答は未確認です。

## 音楽の骨格をSAA、荒れる層をSNへ

| 音源 | 活かす機能 | 使い方 |
|---|---|---|
| SAA1099 | 6声の左右独立音量、2つのハードウェア包絡、トーンとノイズの重ね合わせ | コード、パッド、定位が動くリード、減衰するベル。SNを変調しても音程を保つ層 |
| SN76489AN×2 | 対数音量、白色・周期ノイズ、トーン3連動ノイズ、可変の外部クロック | ステレオのベース、周期ノイズの金属的な音、高速変調のリード、スネア／ハット |

両方とも基本のトーン波形は矩形波です。音色の方向性は、包絡・ノイズ・定位・ピッチ変化・クロックの扱いで作ります。
SAAにはソフトウェアADSRとビブラート、HW BELL／HW TRIANGLEには実ICの包絡も使います。
SNには2 dB刻みの音量変換を使います。急な音量変化やクリックを含む実際の質感は試聴で評価します。

SN1は左、SN2は右に固定されています。SN STEREOでは同じノートを同じ番号の左右トーン、または左右ノイズへ割り当てます。
左右の空き・予約・専用クロックを確認してから両方を発音し、片側を他の音に使うときはペアを一緒に止めます。
CC10 Panを左右の音量へ反映し、PAIR WIDTHで左右に小さなピッチ差を作れます。
WIDTHは0–30 centsの合計差で、分周器の丸めにより実際の差は目標値と一致しない場合があります。ベースの初期値は0です。

## 初期のMIDI割り当て

USBとTRSを独立して設定できます。新規設定はどちらもCHANNEL MAP＝BY CHANNELです。

| MIDIチャンネル | 音源 | 初期音色／用途 |
|---|---|---|
| 1 | SAA | STEREO PAD：コード・持続音 |
| 2 | SN STEREO | WARM BASS：2 MHz基準のベース |
| 3 | SAA | CHIP LEAD：旋律 |
| 4 | SAA | HW BELL：実ICの減衰包絡 |
| 10 | SN STEREO | ノート36＝KICK、38＝WHITE SNARE、42／44＝HI HAT |
| その他 | OFF | 必要なパートを設定して使う |

SNのノイズペアは1組なので、スネアとハットは互いに声を奪います。クラシックな音源の制約を使ったチョークとして扱えます。
キックはトーンのペアを使い、ベースとトーン枠を共有します。専用の追加ドラム音源ではありません。
Ch10の対応は上記4ノートだけの簡易マッピングで、完全なGM音源ではありません。

OLEDのUSB MIDI／TRS MIDIで次を操作します。

1. CHANNELは入力フィルター。OMNIなら全チャンネルを受けます。
2. CHANNEL MAPをBY CHANNELにする。
3. EDIT CHANNELで1–16を選ぶ。
4. PART TARGETでAUTO／SAA／SN1 LEFT／SN2 RIGHT／LAYER MONO／SN STEREO／SAA+SN STEREO／OFFを選ぶ。
5. PART PATCHで音色を選ぶ。選んだ音源に適合する音色だけが候補になります。

WHOLE INPUTでは従来のTARGET／PATCHを入力全体に使い、PART設定は使いません。
BY CHANNELではPART設定を使い、従来のTARGET／PATCHは使いません。フィルターはどちらでも作用します。
Program Changeは受信チャンネルだけの音色を変え、USBとTRSは独立しています。外部から適合しない音色を選んだ場合は発音を断ります。
設定の変更では消音します。EDIT CHANNELは編集先を選ぶだけで、発音を止めません。
TRSのTO USBはローカルの音源割り当てと独立して転送します。

SAA+SN STEREOは1ノートをSAAの1声＋左右SNの1ペアへ重ねるモードです。3つの発音枠を使います。
HW BELL／HW TRIANGLE／TONE NOISE／PERIODICなど、片方のIC専用の音色は使えません。
発音前に3枠の適合を確認し、構成音の声を他へ使うときはそのノートの3枠をまとめて止めます。
LAYER MONOは旧版互換のSAA＋片側SNで、SAA専用音色ではSAAだけが鳴ります。

## CV／ゲート

CV1＋GATE1、CV2＋GATE2を独立した演奏パートとして使います。
CVは1 V/octの音程、ゲートHighで発音、Lowでリリースです。

| 初期割り当て | 音源 | 音色 | 0 Vの音程 |
|---|---|---|---|
| CV1／GATE1 | SAAのVOICE 0 | PLUCK | C3、MIDI 48 |
| CV2／GATE2 | SNのVOICE 6＋9、左右ペア | WARM BASS | C2、MIDI 36 |

CV/GATEメニューでENABLE、VOICE、PATCH、0V NOTE、SN STEREOを設定します。
SAAは1声のまま左右に出せます。SN STEREOをONにすると、VOICE 6／7／8に右側9／10／11を、VOICE 12に右側13を組にします。
同じゲートで左右が立ち上がり、CVの音程変更・リリース・再トリガーも同時に反映します。
CV ROLEで音程入力の代わりにクロック・音量・定位などの変調入力を選べます。下記の追加機能を参照してください。
CVでSAAとSNをまとめて重ねるモードはありません。

CV RESERVE＝ONでは発音する声に加え、包絡・周期ノイズ用のクロック枠も予約します。
2つのCVが同じ声・専用クロックを取り合う設定は受け付けません。
OFFではMIDIとCVの最後の発音を優先しますが、CVの音程更新だけではMIDIの声を奪い返しません。
CALIBRATEで0 Vと1 Vを校正し、PRESET SAVEで校正・割り当てを保存します。校正値は全スロット共通です。

## SNの基準クロックと変調

SN CLOCKのREFERENCEで4／2／1 MHzを選びます。新規設定の通常値は2 MHzです。
クロック生成と分周値計算の両方に同じ基準を使うため、MODE OFFでA4を送ればA4、CVを1 V上げれば1オクターブ上になります。

| 基準 | 通常矩形波の最低周波数 | 標準ノートの下限 |
|---|---|---|
| 4 MHz | 約122.2 Hz | B2 |
| 2 MHz | 約61.1 Hz | B1 |
| 1 MHz | 約30.5 Hz | B0。低いE1も範囲内 |

低い基準ほど高音側の分周精度は粗くなります。基準変更時は消音し、次の発音から適用します。
BASE DROP／DEPTHは基準からさらにクロックを下げる効果です。音程を補正しないので、ここから音が意図的に崩れます。
実クロックの下限は250 kHzです。2 MHz基準では最大3オクターブ、1 MHz基準では最大2オクターブまで下がり、それ以上の指定は下限で止まります。
詳しいモード・速度・同期は[SN CLOCK](SN_CLOCK.md)を参照してください。

2個のSNはクロックを共有しているので、ベースだけを変調してドラムを完全に安定させることはできません。
安定したベースが必要な場面ではSAAへ割り当て、SNをドラムや効果音に集中させます。
TRIGGER FROMで再開始のきっかけをANY SN／MIDI SN／CV1／CV2／DRUM CH10／SN TONES／SN NOISE／MELODY/CVから選べます。
初期値MELODY/CVは、Ch10の打楽器が毎回ベースのクロックスイープを再開始することを避けます。
CV1／CV2を指定した場合は、そのCVがSAAに割り当てられていてもゲートでSNの変調を再開始できます。SAAの音程は変わりません。
これはトリガーの選別であり、変調が作用する音源を分離する機能ではありません。
ARPEGGIOのAPPLY TOはALL／SAA／SNを選べます。新規設定はSAAで、コード側を巡回させながらSNベースを保てます。

## 8つの初期プリセット

| SLOT | 方針 | 主な変更 |
|---|---|---|
| 1 BALANCED | 基本の演奏 | 上記チャンネル表、SAAプラック＋SNステレオベースのCV |
| 2 WIDE PAD | 広がる持続音 | CV1をSAA PAD、Ch2をSN DRY SQUARE、PAIR WIDTH＝12 cents |
| 3 BELL ARP | ハードウェア包絡 | CV1をSAA VOICE 2・HW BELLへ。クロック用VOICE 1も予約。SAAだけにMINOR ARP、6 clocks/step |
| 4 LOW BASS | 低音 | SN基準1 MHz、SAAプラック、SNステレオベース |
| 5 DIVE DRUM | ドラムの下降 | SN DIVE、2 Hz／DEPTH 36、Ch10で再トリガー。Ch2ベースをSAAへ、CV2は無効 |
| 6 METAL | 周期ノイズの変調 | SN TRIANGLE 160 Hz、CV2をPERIODICへ。Ch2はSN DRY SQUARE、Ch10はSAA。CV2は左右ノイズと第3トーンを予約 |
| 7 BROKEN | 高速ランダム | SN RANDOM 250 Hz／DEPTH 36、Ch2はSN DRY SQUARE |
| 8 CLOCK SEQ | 拍に沿う変化 | SN SEQUENCE、4 MIDI clocks/cycle、Ch2はSN DRY SQUARE |

SLOTは1始まり、PATCH／Program Changeは0始まりです。
SLOTを選んでLOADで読み込み、INITでその枠の初期設定をRAMへ呼び直せます。INITはその枠のカスタム設定を置き換えます。
SAVEするまでフラッシュは変更せず、INITでも共通のCV校正値は維持します。
旧版を移行した場合は4 MHz基準・単声CV・全入力ルーティングを保つので、新しい初期構成はINITから選べます。

## 発音数の読み方

物理枠はSAA6＋SNトーン6＋SNノイズ2＝14です。
SNを左右ペアにした場合は、最大SAA6和音＋SN3和音＝9和音と、ノイズ1組です。
初期設定のCV予約はSAA1声＋SN1ペアなので、MIDI用はSAA5＋SN2ペア＝最大7和音、ノイズ1組。CVでさらに2つのステレオ音を演奏できます。
包絡・周期ノイズのクロック予約、キック、3枠のハイブリッドでは、使える和音数が減ります。全チャンネルは同じ声を共有します。
ホームのNOTESは発音中のノート単位、VOICESは使っている物理枠数で、リリース中も含みます。

技術仕様の参照：[Philips SAA1099](https://bitsavers.org/components/philips/_dataBooks/1986_IC01_Philips_Radio_Audio_and_Associated_Systems.pdf)、[TI SN76489AN](https://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf)。


## v0.4：演奏と音作りを広げる設定

追加機能は初期状態で元の音色を維持します。スケール／グライド／トレモロはOFF、移調は0、音域は0–127です。
3ボタンの操作方法は共通で、ホームの−／＋でメニュー、OKで項目を開き、OKで編集を切り替えます。
保存対象は各プリセット。CV校正だけは全プリセット共通です。

### CVのスケール量子化とグライド

CV/GATE 1・2のSCALEはOFF／CHROMATIC／MAJOR／NATURAL MINOR／DORIAN／MINOR PENTA／MAJOR PENTA／WHOLE TONE。
SCALE ROOTはC–Bで、0V NOTEとは独立です。例えば0V NOTE=C3、SCALE=MINOR PENTA、ROOT=Cなら、CVを動かすだけでCの短調ペンタトニックに沿って演奏します。
入力はMIDI 0–127へ制限し、最も近いスケール音を選びます。同距離では低い音を選択します。
発音中は境界に0.08半音のヒステリシスを入れて小さなADC揺れによる音程の往復を抑えます。新しいゲートでは直近の入力から選び直します。

GLIDEはOFF／10／30／60／120／250／500／1000 ms。表示は平滑化の時定数で、指定時間で必ず移動を完了する値ではありません。
ゲートHighの音程変化へ指数的に追従し、SN左右ペアは同じ音程へ同時に移動します。新しいゲートのアタックはその時点のCV音程から開始します。
リリース中の音程は固定します。量子化の後にグライドするので、移動中はスケール間の音程も通ります。

### CVを変調入力にする

CV ROLEをPITCH以外にすると、その系統は声を発音・予約せず、CVを変調へ使います。
ENABLEは引き続き有効。PITCH用のVOICE／PATCH／SCALEなどは保持し、役割を戻すと再び使用します。
MOD RANGEは0–5 Vまたは0–10 V。範囲外は端で止めます。校正後の電圧を使います。

| CV ROLE | 電圧に対する動作 |
|---|---|
| PITCH 1V/OCT | 元の音程CV＋ゲート。スケール／グライドも使用可 |
| SN BASE DROP | SN基準から0–48半音下へ。MODE OFFなら一時的にMANUALとして動作 |
| SN FX DEPTH | 変調のDEPTHを0–48半音へ。MODE OFF／MANUALでは深度を使わないため効果なし |
| SN PAIR WIDTH | 左右の音程差を0–30 centsへ。SN左右ペアだけに作用 |
| MASTER LEVEL | 保存されたMASTER音量へ0–100%の倍率を掛ける |
| SAA PAN | SAAの定位を左→右へ。音色固有の定位移動も加わる |
| TREMOLO DEPTH | 全音源のトレモロ深度0–100%。速度はTONE EDITのSAA／SN別設定 |

変調入力のゲート立ち上がりは、SNクロック効果の手動トリガーとして扱います。TRIGGER FROM／NOTE RETRIGGERに関わらず再開始します。
同じ変調先へ2系統を設定する競合は拒否します。CVによる実効値はプリセットに書き戻しません。
ADC取得が100 ms以上途切れた場合は変調を外して保存値へ戻し、音程CVの声はリリースします。
ケーブルを抜いたことを識別する回路はないので、未接続で0 Vを測定できる場合は0 Vとして扱います。
CV入力の速度はADS1115と入力フィルターに制限され、オーディオレートのFM入力にはなりません。

### MIDIパートの演奏設定

USB MIDI／TRS MIDIでEDIT CHANNELを選び、追加したPART TRANSPOSE／LOW NOTE／HIGH NOTE／VELOCITYを編集します。
この4項目はWHOLE INPUTでも受信チャンネル別に適用します。移調は−24〜＋24半音、音域は移調前の受信ノート番号で両端を含みます。
チャンネル別の音域フィルターなので、1チャンネルを複数パートに分岐するキーボードスプリットではありません。

VELOCITYはLINEAR／SOFT／HARD／FIXED 110。SOFTは軽く弾いても音量を出しやすく、HARDは弱い入力を抑えます。
非ゼロVelocityは最低1を維持し、Note On Velocity 0は従来どおりNote Offです。
移調してもNote Offとサステインは元の受信ノートに対応します。チャンネル間・USB／TRS間の設定は独立です。

Channel PressureとPolyphonic Key Pressureは追加のビブラートへ反映します。CC1と各Pressureの最大値を使い、音色と同様に発音180 ms後から入ります。
RPN 0（CC101=0、CC100=0）選択後、CC6でベンド幅0–24半音、CC38で追加0–99 centsを設定できます。RPN NullやNRPN選択後のData Entryでは変更しません。
MPEのゾーン／マスターチャンネル合成は実装していません。MIDI 1.0のチャンネル別制御です。

### TONE EDIT：包絡とトレモロ

SOUND GROUPでSAA1099またはSN1+SN2を選び、ATTACK SCALE／DECAY SCALE／SUSTAIN／RELEASE SCALE／TREMOLO／TREMOLO RATEを編集します。
SN1+SN2という表示のグループは、左右ペア・単声を含む両方のSNの全声へ作用します。
時間倍率は25–約395%、元の音色と同じ値は100%（内部値64）。SUSTAINのPATCH VALUEは元の音色、0–100は明示した維持レベルです。
ソフトウェアADSRへ作用し、SAAのハードウェア包絡の形・70 Hz専用クロックは変更しません。
MIDIチャンネルごとのCC73 Attack／75 Decay／72 Releaseは同じ倍率をさらに掛け、各段階の時間は1–8000 msへ制限します。
CC70は本機独自のSustain override（0–127→0–100%）。CC121でチャンネル側の倍率・Sustain override・Pressureを初期値に戻します。
これらのMIDI CC状態は保存しません。TONE EDIT側の設定は保存します。

TREMOLOは0–100%の音量変調、RATEは0.5／1／2／4／8／16／32／64 Hz。
両側を同時に動かすため、SNの左右ペアを維持します。SNは2 dB刻み、SAAは4bit音量なので、段階的な質感になります。
音声波形へDSPエフェクトを掛ける構成ではありません。

### 内部テンポと8ステップのSNクロック

ARPEGGIOにINTERNAL BPM（30–240）とCLOCK MODEを追加しました。
AUTOは選択したUSB／TRSのClockが届く間は外部に追従し、600 ms途切れると内部テンポへ戻ります。
外部Stopを受けた場合は停止を維持し、Continue／Startで再開します。欠落からの切替時に位相を厳密に接続する機能はありません。
EXTERNALは外部Clockだけ、INTERNALは内部テンポだけを使い、外部Clock／Start／Stop／Continueを無視します。
INTERNALは電源投入・プリセット読込・CLOCK MODE変更で走り始めます。テンポはUSBへ送信しません。

CHORDはOFF／MAJOR／MINOR／OCTAVEに加えてMAJOR 7／MINOR 7／MINOR 6／POWER。
押さえた実際のコードを巡回する方式ではなく、各発音の音程を4段階の決まった間隔で変えるチップチューン向けの機能です。

CLOCK STEPSでSTEP 1–8とDROP PERCENTを編集します。SN CLOCKのSEQUENCEで、BASE DROP＋DEPTH×各ステップの割合を実クロックへ反映します。
同期の場合、MIDI CLOCKS/CYCLEは8ステップ全体の周期です。1周期に8未満のClockなら一部ステップを飛び越えます。
FREEはRATEを全体の巡回速度として使います。AUTO／INTERNALを使えば、MIDI接続なしでも同期モードのパターンを動かせます。
CC30（値0–7）でステップ、CC31（値0–127→0–100%）でその値を編集できます。SN CLOCKのCLOCK/CC SOURCEで選択した入力だけが有効です。

### 設定の移行

保存形式はv4。v1／v2／v3のCRCと値を検証し、音色・割当・CV校正・v3の左右ペア／基準クロックを保持してRAM上へ読み込みます。
追加機能は元の音に影響しない初期値にします。SAVEするまでは旧形式のフラッシュを変更しません。
v0.4でSAVEしたv4形式はv0.3以前では読めないため、旧版へ戻す場合は設定を別途控えてください。

### 演奏例

- **CVメロディ＋荒れるSN**：CV1=PITCH、MINOR PENTA、GLIDE 30 ms。CV2=SN FX DEPTH、SN CLOCK=TRIANGLE。CV1のSAAメロディを維持し、CV2でSNの崩れ方を変える。
- **左右でうなるベース**：Ch2=SN STEREO、CV2=SN PAIR WIDTH。0 Vでは左右同音、上げると微小な差が付く。CV2の元のベース予約がなくなるのでMIDI用のペアが増える。
- **レトロな震えるコード**：Ch1=SAA PAD、TONE EDITのSAA TREMOLO=40%、RATE=4 Hz。SNベースのトレモロは0%で安定させる。
- **クロックの階段**：SN SEQUENCE、DEPTH=24、CLOCK STEPS=0/0/50/50/100/100/50/0、INTERNAL=120 BPM、24 clocks/cycle。全SNの速度を1拍で巡回させる。

MIDIメッセージの参照：[MIDI AssociationのCC／RPN一覧](https://midi.org/midi-1-0-control-change-messages)、[MIDI 1.0メッセージ一覧](https://midi.org/summary-of-midi-1-0-messages)。
