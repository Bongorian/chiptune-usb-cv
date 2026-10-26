# Wave Lab USB MIDI protocol 1

v0.8.0-alpha以降。USB MIDI cable 0のみ。TRS入力はエディタコマンドを実行しません。通常のMIDI演奏は従来通りです。0x7Dは開発用の非商用SysEx IDです。

```
F0 7D 43 54 01 request-id command payload... checksum F7
```

`F0/F7`以外は7ビット。`7D`からchecksumまでの和がmod128で0。応答は同じrequest-id、command OR 0x40、payloadの先頭にstatus。status=0成功、1形式／バージョン不正、2範囲外／音源役割不一致／回路競合、3編集中または電源OFF、4保存失敗。未認識のメーカーID、不完全・長過ぎるフレームは破棄します。

送信側は一度に1要求とし、応答後に次を送ります。受信最大192バイト、途中のリアルタイムメッセージは無視し、部分フレームは無通信2秒で破棄します。チェックサム不一致は音色を変更しません。応答は完全なUSB MIDI SysExパケット列として送ります。TRS→USB転送中のSysExがある場合は終端を待ち、メッセージを入れ子にしません。UIの数値編集中・確認中・名前編集中は書き込みを拒否し、読み出しはできます。

| Command | 要求 | 成功応答（statusの後） |
|---|---|---|
| 0 HELLO | なし | firmware major/minor/patch、保存形式、パート数、USER数、FACTORY数、音色パラメーター数 |
| 1 GET_PART | part | part data＋出力マスク＋予約マスク |
| 2 SET_PART | part data | 正規化したpart data＋2マスク。全検証後に反映 |
| 3 SAVE | part、USER slot | なし。対象音色＋現在の全パート・ルート・クロックを保存。演奏停止。失敗時は保存用RAMも復元 |
| 4 GET_CLOCK | なし | reference、mode、base、depth、rate、sync、retrigger、width、trigger source、clock steps×8 |
| 5 SET_CLOCK | 上記17バイト | 上記17バイト。左右全SNで共有。reference変更時は演奏停止 |
| 6 PROFILE | 0 HYBRID／1 SAA POLY／2 DUAL LAYER／3 PERCUSSION／4 MIDI 10（v0.9以降） | なし。全パート・ルートを置換し演奏停止 |
| 7 AUDITION | part、note、mode（0 OFF／1 SINGLE／2 REPEAT／3 HOLD） | なし |
| 8 GET_MOTION | part | motion data |
| 9 SET_MOTION | motion data | motion data。STOPは全体停止 |
| 10 GET_CV | input（0/1） | CV data |
| 11 SET_CV | CV data、またはinput 0と1のCV dataを連結 | 同形式。2系統連結時は両方をまとめて検証・反映 |
| 12 FACTORY | factory index | index、name×8、sound data。本体の現在音色は変更しない |

番号はすべて0始まり。`part data`はpart、enabled、poly、role、MIDI channel、ASCII name×8、sound data。channel=127ならルートは変更しません。GETでは複数のUSB受信チャンネルがある場合に127を返します。SETで0–15を指定すると、そのパートのUSB/TRS受信先を指定の1チャンネルへ置き換えます。

`sound data`はパラメーター数1バイト＋各値を2バイトずつ。順序・型・範囲は[parameters.json](parameters.json)が正本です。各値は符号の有無にかかわらず`value + 8192`を下位7ビット→上位7ビットで表します。stepsは8か16のみ。マスクはオフセットを付けない14ビット値です。

`motion data`はpart、length、division、direction、sequence、arp、arp_octaves、running、note×16、gate×16、velocity×16、motion×16。motionの各値のみ上記の2バイト符号付き形式、他は1バイト。合計88バイトです。

`CV data`はinput、mode、part、base_note、scale、root、glide、range、gate、gate_part、parameter、gate_note、amount。amountのみ2バイト符号付き形式。合計14バイト。

音色・ステップ・クロック・CVの反映はRAMだけを変更します。永続化は明示的なSAVEのみ。音色JSONには1パートの音色とステップを収録し、クロック・CVは含みません。固定役割や他パートとの資源競合は本体側でも検証します。

`tools/generate_editor.py`でCとJSのフィールド定義を生成し、ホストビルドの`editor_export`で`editor/factory.json`を生成します。JSのテストは`editor_bridge`へ実フレームを送ってC側との往復を検証します。ブラウザーテストのMIDIポートはこのCプログラムを背後にしたテスト用ポートで、実USB・実音の検証とは区別します。
