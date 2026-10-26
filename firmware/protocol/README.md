# Wave Lab USB MIDI protocol 2 / schema 10

v0.10.0専用。USB MIDI cable 0のみ。TRSは通常MIDI用で、エディタ要求を実行しません。旧protocol 1、旧JSON、旧保存形式との互換処理はありません。`0x7D`は開発用の非商用SysEx IDです。

```
request:  F0 7D 43 54 02 id cmd       payload... checksum F7
response: F0 7D 43 54 02 id (cmd|40h) status data... checksum F7
```

F0/F7以外は7 bit。7Dからchecksumまでの和をmod128で0にします。一度に1要求、応答後に次の要求を送ります。最大192 bytes、部分フレームは2秒の無通信で破棄。途中のリアルタイムメッセージはフレームへ含めません。id=0–127、cmd=0–63。応答のstatusは0=成功、1=形式/バージョン、2=範囲/資源/構造、3=UI編集中または音源電源OFF、4=保存失敗、5=未交渉/schema不一致です。

書込前に必ずHELLOへ`[10]`を送信し、応答のfirmware=0.10.0/protocol=2/schema=10を完全一致で確認します。設定変更、保存、試聴、PANIC、テンプレート、POLY FOCUSには交渉済み状態が必要です。受信側も交渉前の変更を拒否します。読出しだけでは交渉済みになりません。USBポート再接続時は再交渉してください。

整数の表記:

- `u14(v)` = `[v & 127, v >> 7]`、下位から。
- `s14(v)` = `u14(v + 8192)`。
- 16 bit MIDIマスク = `[v & 127, (v >> 7) & 127, (v >> 14) & 3]`。
- `u32(v)` = 下位7 bitずつ5 bytes。
- 名前は印字可能ASCII 8文字、空白埋め。Cの終端NULは送信しません。
- Track、slot、input、factory、channelはすべて0始まり。

## コマンド

| cmd | 要求payload | 成功data |
|---|---|---|
| 0 HELLO | `[10]` | `[0,10,0,2,10,14,16,16,4,23,F]` |
| 1 GET_TRACK | `[track]` | Track＋割当マスク |
| 2 SET_TRACK | Track（割当マスクなし） | Track＋割当マスク |
| 3 SAVE | `[kind,track,slot]` kind=0Patch/1Track/2Project/3System | 空 |
| 4 GET_GLOBAL | 空 | Global |
| 5 SET_GLOBAL | Global | 適用後Global |
| 6 LOAD_PROJECT | `[slot]` | 空 |
| 7 AUDITION | `[track,note,mode]` 0stop/1once/2repeat/3hold | 空 |
| 8 GET_SEQUENCE | `[track]` | Sequence |
| 9 SET_SEQUENCE | Sequence | 適用後Sequence |
| 10 GET_CV | `[input]` 127=両方 | CV、またはCV0＋CV1 |
| 11 SET_CV | CV、またはCV0＋CV1（異なるinput） | 適用後CV、またはCV0＋CV1 |
| 12 GET_FACTORY | `[factory]` | `[factory,name8,Patch]` |
| 13 GET_PATCH | `[slot]` | `[slot,name8,Patch]` |
| 14 LOAD_PATCH | `[track,slot]` | Track＋割当マスク |
| 15 GET_MATRIX | 空 | ModRoute×8 |
| 16 SET_MATRIX | ModRoute×8 | 空 |
| 17 GET_SYSTEM | 空 | `[brightness]` |
| 18 SET_SYSTEM | `[brightness]` 10–100 | 空（RAMのみ） |
| 19 PANIC | 空 | 空 |
| 20 LOAD_TRACK | `[track,slot]` | Track＋割当マスク |
| 21 DIAGNOSTICS | 空 | 診断値 |
| 22 TEMPLATE | `[0 MIDI10 / 1 SAA6poly / 2 Tone+ENV]` | 空 |
| 24 POLY_FOCUS | `[track]` | Track＋割当マスク |

Fは[parameters.json](parameters.json)のフィールド数（現在35）。この順序からCとJSを生成します。全パッチ値は型に関係なくs14で運び、受信時に型・範囲・組合せを検証します。生レジスタ書込コマンドはありません。SETは候補Projectを検証後に一括適用し、失敗時は元の構成と発音を保持します。

## Patch / Track

Patch = `[F, s14(field0), ... s14(fieldF-1)]`。

Track prefix（38 bytes）:

```
track enabled mode voices level pan mute low high velocity_curve morph_target morph
name[8]
macro_dest[4]
s14(macro[0..3])
mask16(USB receive channels) mask16(TRS receive channels)
```

mode=0DIRECT/1POLY/2LEGATO、voices=1–6、level/pan/morph=0–100、morph_target=0off/1–16USER。送信はprefix＋Patch。応答はさらに`u14(audible_outputs), u14(reserved_resources)`を追加します。長さは送信`39+2F`、応答`43+2F` bytes。

マスクのbit0–5=SAA1–6、6–8=SN L1–3、9–11=SN R1–3、12=SN L noise、13=SN R noise。resources−outputsが無音の専用クロックです。声数分の和集合を返します。POLY FOCUSは同じチップの他TrackをOFFにする明示操作で、別チップの発音は保持します。

## Global（26 bytes）

```
sn_reference mode base depth rate sync retrigger trigger_source clock_source clock_policy
u14(tempo_minus_30)
midi_thru mix[4] clock_sequence[8] gate_clock_input
```

`sn_reference`=0 4MHz/1 2MHz/2 1MHz。tempoは0–210で30–240 BPM。tempoだけは2 bytesです（7 bitへ切り詰めないこと）。gate_clock_input=0/1。mix=Master/SAA/SN L/SN R、各0–100。clock_sequenceも0–100。

## Sequence / CV / Matrix

Sequence（87 bytes）:
`[track,enabled,length,division,direction,arp,arp_octaves,note[16],gate[16],velocity[16],s14(motion[16])]`。
length=1–16、division=1–24 MIDI clocks、direction=forward/reverse/random、arp=off/up/down/updown/random/chord、arp_octaves=1–4。motion=−100～100。

CV（14 bytes）:
`[input,mode,track,base_note,scale,root,glide,range,gate,gate_track,parameter,gate_note,s14(amount)]`。
mode=0off/1pitch/2mod、range=0 5V/1 10V、gate=0off/1note/2retrigger/3step/4clockFX。2入力の変更は28 bytesの一括SETが可能です。

ModRoute（5 bytes）: `[source,track,parameter,s14(amount)]`。
source=0off/1CV1/2CV2/3CC1/4pressure/5–18各TrackのLFO/19–32各Trackのstep。pressureはTrackに届くchannel/active-note pressureの最大値。parameterはmodel.hのModParameter（0–11）です。

## 診断とエラー

DIAGNOSTICS（33 bytes）:
`[active_voices,u32(steals),u32(refused),u32(register_writes),u32(coalesced),pending,max_pending,u32(tick_max_us),u32(tick_overruns)]`。
実機の時間カウンター以外をホストの時間測定と混同しないでください。

status 2のdata:
`[track,other_or_127,reason,available,reason_length,reason_ASCII...,suggestion_length,suggestion_ASCII...]`。
reason=1invalid/2capacity/3sharedNoise/4clockBusy/5morph。other=127は特定の競合元を指していないことを表します。availableはその割当試行での空き回路数で、実際に追加できる声数ではありません。

Project読込・テンプレートは全体を停止します。Patch/Track構造変更は対象だけを停止し、競合で別Trackを強制停止しません。音色保存だけでProject・Systemは変わりません。保存成功の応答は検証済みバンクへのコミット完了後です。キャリブレーション値は本体UIから設定し、System保存で永続化します。
