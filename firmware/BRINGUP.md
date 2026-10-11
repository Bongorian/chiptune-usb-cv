# 初回動作確認

v0.2.0-alphaは実基板で評価するための初版です。確認した結果と測定値を記録し、音量を上げる前に電源とクロックを確認してください。

1. 音源IC・ソケット・OLEDの向きを照合する。SNは**SN76489AN**を使う。基板電源OFFで音源ICを装着する。
2. PCとUSB-Cで接続し、空フラッシュなら`RPI-RP2`へUF2を書き込む。以後の更新は−と＋を押しながら接続する。
3. PC上で`SAA1099 + Dual SN76489AN`がUSB MIDIとして列挙されることを確認する。入力・出力が1ポートずつ見える。Linuxでは`lsusb -d cafe:4008`、`amidi -l`などで確認できる。
4. 待機時の3.2 Vロジック・5 V音源電源、USBからの電流を測る。未構成／サスペンド時は音源電源がOFF、信号線がHi-Z、表示とLEDがOFFになることを確認する。USB規格に収まる電流かは実測で判定する。
5. オシロスコープでSAA CLK＝8 MHz、SN CLOCKのMODE OFFでSN CLK＝4 MHz、約50%デューティを確認する。SNのWE LOWはOFFで16 µs以上、最低クロックで136 µs以上、SAAのアドレスとデータの順序・待ち時間を確認する。
6. 外部アンプの音量を下げ、10 kΩ以上の入力のアンプへLINE OUTを接続する。MASTERを低く設定する。USB MIDIでノート69、Velocity 80を送り、Note Offで止まることを確認する。
7. TARGETをSAA→SN1→SN2と切り替え、SAAの左右、SN1の左、SN2の右から音が出ることを確認する。SNの通常矩形波で低音がクランプすること、PERIODICのノート69が概ねA4になることも確認する。
8. SAAでHW BELL／HW TRIANGLE、TONE NOISE、STEREO PADを試す。包絡の実際の周期・開始状態、ステレオ反転、量子化ノイズ・クリックを確認する。
9. TRS Type A MIDIを接続し、USBと同時に演奏する。サステイン、Pitch Bend、CC、同音の重複発音、PANICを確認する。TO USBではPC側のMIDIモニターでノート・CC・SysExの転送を確認する。
10. CV1とCV2を0 V／1 Vで順に校正し、SAVEする。0 V、1 V、2 V、負のCVで音程を測る。ゲート0–5 V、0–10 Vの立ち上がり・立ち下がりが2系統独立して働くか確認する。
11. 声の予約ON／OFF、CVとMIDIの同時演奏、トーン連動ノイズのクロック予約、MIDIの高負荷で異なる声の音色が変わらないことを確認する。
12. SAVE→USB切断→再接続→LOADで設定・校正が残ることを確認する。DIAGNOSTICSのOVERRUN／UART ERROR／USB DROPを記録する。
13. 1.5 A以上を広告するUSB-C充電器でCV／TRSの単独動作を確認する。通常電流広告のUSB-A充電器では音源電源を入れない仕様。

I2CのOLED／ADCが見つからない場合は、それぞれ0x3C／0x48、3.2 V給電、SDA/SCLとGNDを確認する。
OLEDが見つからなくてもUSB MIDIの音源エンジンは動く。ADC異常時はCVの声をリリースする。
音源ICがない場合もハングしないが、DTACK／READYで存在確認を行う実装ではないため、画面にIC未装着とは表示しない。

評価記録には、基板リビジョン、音源ICの刻印、OLED型番、ファームウェアのハッシュ、USBホスト／充電器、各電圧・電流、音程誤差、最大処理時間、更新遅れを残す。

SN CLOCKの追加確認:

- MASTERを下げ、SN1／SN2それぞれでMODE OFF→MANUAL、BASE 0／12／24／36／48を操作する。SN CLOCK端子が4／2／1／0.5／0.25 MHz、定常時50%になること、SAAが8 MHzを維持することを測る。
- TRIANGLE、SQUARE、RANDOM、SEQUENCEのDEPTHとRATEを変え、短すぎるパルスや4 MHzを超えるパルスがないか測る。800 Hz設定時のタイマー更新とMIDI高負荷時のジッターを記録する。
- SNの左右のトーンと固定レート／周期ノイズが同時に変化し、SAAの音程が変わらないことを確認する。RISE／DIVEの再トリガー、CVゲートの再トリガー、MIDI同期とStop、CC20–26を確認する。
- 250 kHz付近で連続したレジスター更新が取りこぼされないかを確認する。低速クロック・多声・ビブラートを組み合わせ、USB受信、CVとOLEDの動作、DIAGNOSTICSの遅れを記録する。
- MODEやBASEなどをSAVE→電源入れ直しで復元する。v0.1の保存領域がある場合は音色・ルーティング・CV校正を保持し、SN CLOCK OFFで移行することを確認する。
