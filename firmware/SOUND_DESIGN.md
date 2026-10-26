# 音色の構造と回路の予約 — v0.9

音色は「発音構造＋音量包絡＋変調」です。MIDIチャンネルは音色を演奏するパートへの入力で、物理ICや波形そのものではありません。本体の3ボタンだけでも作成・保存できます。Webエディタは同じ音色データを編集します。

## 発音構造

内部回路番号はこの資料では1始まりです。

| 構造 | 発音 | 1音の回路使用量／共有 |
|---|---|---|
| SAA TONE | 矩形波 | SAA出力1回路、左右独立音量 |
| SAA NOISE | ノイズ | 出力1回路。レートは内部1–3／4–6のグループごとに共有 |
| SAA MIX | トーン＋ノイズ | 出力1回路。同一出力の両成分に独立ADSRや連続ミックス比は付けない |
| SAA ENV | ENVを楽音波形として出力 | 出力3または6＋クロック2または5の2回路 |
| SAA AM | トーン×ハードウェアENV | 同上。ENV RATIO STがノート周波数に対するクロックの半音差 |
| ENV NOISE | ノイズ×ハードウェアENV | 上記2回路。KEY TRACKではノイズクロック1／4も予約して3回路 |
| ENV MIX | トーン＋ノイズをENVで変調 | ENV NOISEと同じ予約。自由なトーン／ノイズの混合率はなし |
| SAA LAYER | ENVの楽音＋別回路のトーン／ノイズ／両方 | 3回路を1組。最大2音。補助回路は音程差・レベル・追加減衰を持つ |
| SN TWIN | 左右の同番号トーンをペア化 | 2回路でステレオ1音。最大3音 |
| SN LEFT / RIGHT | 片側のトーン | 片側1回路 |
| SN NOISE | 左右のノイズをペア化 | ノイズ各1基。KEY TRACKでは左右のトーン3も予約 |
| SN METAL | トーン3と連動ノイズを混合 | 左右トーン3＋ノイズ。WHITE切替も可能 |
| SN STACK | 2／3組のステレオトーンを重ねる | 4／6トーン回路で1音。第2・第3の音程差を指定、各成分のゲインを組数で割る |

SAA LAYERの補助音は全体ソフトADSRに従い、さらに `exp(-経過ms / LAYER DECAY MS)` で減衰します。0msは追加減衰なしです。完全に独立した2つのADSRではありません。補助音だけの遅いアタックなどは別パートのレイヤーで作ります。クロック用回路は通常の発音枠として重複使用しません。

固定レートのSAAノイズは2グループの色を共有します。出力の音量・ADSRは別々ですが、同じグループの音をそれぞれ異なるレートにはできません。KEY TRACKは時計用トーンを専有する場合があります。予約を検証し、成立しない変更は拒否します。音色読込では対象パートの声数を減らして成立させる場合がありますが、他パートの予約は奪いません。

ハードウェアENVは内蔵8形状・8／16段階です。波形RAMへの自由描画や任意のPCM再生ではありません。ENVの形状・ステップ数によって周期が変わるため、純ENVの音程計算も連動します。8MHz時の16段三角波は最高約488Hz、上昇／下降波は約977Hz、8段はその倍です。純ENV／LAYERの範囲外のENV出力は消音します。通常トーンのデューティ比は固定です。

## 初期配置 MIDI 10

v0.9の初期値は、各MIDI chを単音とするMIDI 10です。ch1–6=SAAの各1音、ch7–9=左右SNの同番号トーン、ch10=左右SNのノイズ。SNは同音程をL/Rへ送り、PAN中央で両側同音量、CC10で左右へ振ります。初期音色はF21 SAA TONE、F22 SN MONO、F23 SN NOISE。初期試聴はnote 45=A2=110 Hzです。

以下のHYBRIDなどは、特殊発音やポリフォニック演奏へ切り替える追加配置です。[初期配置と特殊MIDI chの一覧](README.md)／[音量補正と周波数表](LEVELS_AND_PITCH.md)。

## 回路配置プロファイル

本体では `OK長押し > PARTS / SETUP > PROFILE > APPLY PROFILE`。全パート・ルートを置き換える確認が出ます。適用時は演奏を停止します。保存するまでRAM内の変更です。

| プロファイル | SAAの配置 | MIDI入力 |
|---|---|---|
| HYBRID | 通常2声＋ENV2声 | ch1通常、ch2 ENV |
| SAA POLY | 通常6声 | ch1、ch2はOFF |
| DUAL LAYER | 複合音色2声 | ch1、ch2はOFF |
| PERCUSSION | 通常2声＋ENV1声＋ノイズ2声 | ch1通常、ch2 ENV、ch11 SAAノイズ |

上記4つの追加プロファイルではch3はSNステレオ2声、ch10はSNステレオノイズ1声です。その他のチャンネルは未割り当て。CV/Gateは初期OFFです。DUAL LAYERではch1の音色はATK BASSをPOLYで使用します。

SN STACKの2組構成はch3で使えます。3組構成はトーン3連動ノイズと両立しないため、ch10を固定レートにするかOFFにします。既存ch10の予約を自動解除しません。

## 音作りの例

- **ENVベース＋アタック**：DUAL LAYERを適用。F19 ATK BASS。LAYER SOURCE=NOISE、LAYER LEVEL=40–60、LAYER DECAY=20–60ms。ベースのADSRとは別に補助成分が先に消えます。
- **ENVで刻むノイズ**：ch2へF17 ENV HISS。形状・ENV RATIO ST・NOISE RATEを変えます。これはノイズに振幅変調をかける音で、純ENV発振の音程とは異なります。
- **ざらついたリード**：F18 ENV GRIT。ENV MIXでトーン／ノイズを有効化し、ENV RATIO STを0、7、12などに設定します。
- **ステレオ和音リード**：ch3へF20 SN STACK。SECOND ST=7、STACK SIZE=2。微小DETUNEで左右差を加えます。
- **クロックを崩す**：SN PERIODIC／METALに共通クロックのRANDOM／DIVEを追加。SAA側で安定した音程を支えると分離して演奏できます。

音の表現は設計上の狙いです。実機での聴感・音量バランスは未評価です。

## 本体UIとWeb

短いOKでSOUND、STRUCTUREで発音方式・波形・レイヤーを選びます。AMP ENVELOPEはノート全体のソフトADSR、ハードウェアENVのWAVE／STEPSとは独立です。レイヤーのレベル・追加減衰・音程差、ENV RATIOもマクロ／CV／変調マトリクスの対象です。ENV/STACKの追加音色はF17–F20、基本音色はF21–F23、USER保存は16枠です。

[Wave Lab](https://bongorian.github.io/chiptune-usb-cv/)はHTTPS上のWeb MIDIエディタです。v0.8以降の本体へUSB接続し、SysExを許可して、入出力両ポートを選択します。対応ブラウザーではAndroidからも同じ画面を使えます。Web MIDI非対応ブラウザーでもオフラインの編集・JSON保存はできます。

波形図は模式図で、ICのノイズ系列・位相・アナログ出力を測ったものではありません。自由に描ける16ステップはMacro 1の時間変化で、オーディオ波形そのものではありません。本体への反映とフラッシュ保存は別操作です。音は基板のLINE OUTから出ます。

資料：[Philips SAA1099](https://sam.speccy.cz/systech/saa-1099_data_sheet.pdf)、[TI SN76489AN](https://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf)。
