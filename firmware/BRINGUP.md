# v0.10 実機確認項目

ここにある項目は**未確認**です。ホスト試験、ARM命令実行、UF2構造検証とは別に測定結果を残してください。この作業では実機書込を行っていません。[復旧](BOOT_RECOVERY.md)と[音量測定](LEVELS_AND_PITCH.md)も参照してください。

## 起動・電源・保存

- コールド起動、UF2直後、USB抜き差し、Mac/PCのスリープ復帰でUSB MIDIとOLEDを個別に確認。
- USB列挙／CC電流広告／サスペンドに応じた音源電源、電流・突入電流、異常電源時の停止を測定。
- 旧形式、空フラッシュ、OK保持でも起動し、自動erase/programしないことを確認。旧校正は移行しないため再校正する。
- Patch保存だけでProject・Track配置・Systemが変わらないこと、Track/Project/System保存の範囲と再起動後の復元を確認。
- バンク消去・データページ書込・コミット前後の電源断で直前の有効保存へ復帰すること。実フラッシュの部分プログラム・保持も確認。

## 演奏と操作

- MIDI10でch1–6がSAA、ch7–9がSN左右同ノート、ch10が左右ノイズ。CC10の左右端と中央、複数chの離鍵独立を確認。
- USB/TRS同時の同音連打、重複Note On/Off、Sustain、RPN0 Pitch Bend、Program Change、Macro CC16–19、Pressure変調、All Notes/Sound Offを確認。
- DIRECT/LEGATO復帰、POLYスティール、保持鍵復帰、PANIC後の無音。POLY FOCUSは同じチップだけ停止し別チップを保持すること。
- 音色変更・声数増加・ENVクロック・共有ノイズ・SN Tone3の競合を拒否し、元の発音を変えないこと。
- OLEDの全ページ、編集確定／取消、長押し加速、保存確認、ミュート、PANIC（−＋保持）、USB UPDATEを操作。
- Wave Labの本体読出し／反映／LIVE、オフライン編集、旧版への書込ロック、切断再接続、保存各種を確認。

## 音源とタイミング

- SAA=8 MHz、SN REFERENCE=4/2/1 MHz、FX範囲250 kHz～4 MHzとデューティを測定。両SNは共通クロック。
- note45=110 Hz、69=440 Hzほか全域、範囲外クランプ、音量補正・PAN・左右差・同時発音時の飽和とヘッドルームを測定。
- SAA ENVの8形状・8/16段・単発／反復・R反転・Retrigger、AM比、補助Layerとその減衰。クロック用2/5が無音であること。
- SAA固定／Key Track Noise、3声群での共有速度、Mixのキャリアがノイズピッチで意図せず変わらないこと。ENV補助Mixは自身のクロックと結び付く。
- SN White/Periodic、固定3レート、Tone3連動、Metal、Layers1–3、左右単独／並列／detuneを測定。周期ノイズのAN型15段比と実個体を照合。
- ロジックアナライザーでSN /WE LOW 16–136 µs、setup/hold、divider2 bytes、クロック変化中の書込、SAA /WRと/CSを確認。
- 14独立声＋ADSR/Pitch/LFO/Morph/8変調、MIDI両入力の高負荷、CV/Gate、OLED編集、低クロックを重ねる。診断`tick_max_us`と`tick_overruns`、実ノート遅延、最悪のバス待ちを記録。**1 msの実時間保証はこの測定後に判断する。**

## CV・Gate・Sequence

- CV1/2を0V→1Vで校正しSystem保存。0/1/2V、5/10V設定、0V発音、量子化ヒステリシス、Glide、変調符号を確認。
- Gate Low/High、Note/Retrigger/Step/Clock FX、Gate Clock入力1/2、PANIC後Low→High復帰、ADC欠落時の停止。
- MIDI Start/Continue/Stop、Auto/External/Internal/Gate Clock、30–240 BPM、16ステップとArpeggio、Sustain解除を確認。

[Verification.json](dist/Verification.json)、[Boot_Verification.json](dist/Boot_Verification.json)、[Runtime_Verification.json](dist/Runtime_Verification.json)、[Test_Verification.json](dist/Test_Verification.json)は自動検証の記録です。未測定項目をそこから実機確認済みとは解釈しないでください。
