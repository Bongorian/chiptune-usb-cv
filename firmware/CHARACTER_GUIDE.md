# 音源のキャラクターと演奏設定

v0.3.0-alphaの演奏用ガイドです。以下は各ICの機能を使う音作りの方針です。
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
現状、CVでSAAとSNをまとめて重ねるモードや、CVを音量・クロック深度へ直接割り当てる機能はありません。

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
