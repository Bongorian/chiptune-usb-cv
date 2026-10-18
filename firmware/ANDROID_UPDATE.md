# AndroidからUF2を更新する

USBホスト/OTGに対応するAndroid端末と、データ通信できるUSBケーブルを使います。USB-C接続でも充電専用ケーブルは使用できません。必要ならOTGアダプターを使います。端末が給電不足になる場合は給電付きUSBハブを利用します。

この基板用のファイルは[**chiptune-v0.6.1-alpha.uf2**](dist/chiptune-v0.6.1-alpha.uf2)です。GitHubでファイルを開いた場合はDownloadから、先にAndroidのDownloadsへ保存してください。Pico/Pico W用やRP2350用のファイルは使いません。

## 1. 本体をBOOTSELにする

いずれかの方法を使います。

- **書き込み済みのファームウェアから**：USBをいったん外し、本体の**−と＋を両方押したまま**Androidへ接続します。USB機器として認識されたらボタンを離します。この起動時の方法はv0.4でも利用できます。
- **メニューから**：v0.6ではホームでOK長押し → `SYSTEM > USB UPDATE > YES`を選びます。未保存の作業は先にSAVEしてください。発音を停止して、RP2040のROMの更新モードへ移ります。
- **復旧時**：電源を外し、基板のJP1（BOOT）を短絡した状態で接続します。更新モードに入ったら短絡を外します。JP2はRESETです。

BOOTSELのUSBドライブ名は`RPI-RP2`、WebUSBの選択画面では`RP2 Boot`などと表示されます。通常のUSB MIDI機器が表示されている状態ではROMへの書き込みはできません。BOOTSELはRP2040のROM内にあります。

## 2. ChromeとWebUSBで更新する方法

AndroidのファイルアプリがRPI-RP2へ書き込めない場合にも使える方法です。

1. Androidの**Chrome**で[**pico⚡flash**](https://picoflash.org/)を開きます。
2. 本体を上記のBOOTSEL状態で接続します。
3. **Connect**を押し、`RP2 Boot`を選択します。Android/ChromeのUSBアクセス許可が出たら許可します。
4. **Flash**画面のファイル欄から、Downloadsに保存したこの基板用UF2を選択します。
5. **Flash Device**を押して完了表示を待ちます。
6. **Reboot**で通常起動します。切断済みなどで実行できなければ、ボタンを離した状態でUSBを抜き差しします。
7. ホームでOK長押し → `SYSTEM > VERSION`で`V0.6.1 ALPHA`を確認します。MIDI ch1で弾くか、メニューの`TOOLS / AUDITION`でTESTをREPEATにして音を確認します。

通常のFlash画面を使います。AdvancedのErase Flashや全フラッシュ消去は、通常の更新には不要です。UF2とアプリケーション部分だけを更新し、設定用の末尾64KiBを残します。新しいUF2は旧v0.4の校正領域も含みません。

pico⚡flashの開発元はRP2040とAndroid/Chromium/WebUSBへの対応を記載しています。本プロジェクトの端末実機での更新は未確認です。インストール先へファームウェアや設定を保存する操作は、このブラウザーとUSB機器の間で行います。

## 3. USBドライブとしてコピーする方法

端末のファイルアプリがUSBストレージへの書き込みに対応し、`RPI-RP2`が表示される場合は、UF2をそのドライブの直下へ**コピー**できます。書き込み完了時には本体が再起動し、ドライブが消えます。通常のUSBメモリーのようにUF2が残ることはありません。

端末によって表示や書き込み対応が異なります。ドライブが出ない、コピーできない場合はChromeのWebUSB方式を使います。どちらも使えない場合はPCから同じUF2をRPI-RP2へコピーできます。

## 接続できないとき

- ChromeのUSB選択に出ない：本体の通常MIDIモードではなくBOOTSELか、ケーブルがデータ対応か、AndroidがUSBホストになっているかを確認します。
- 別のアプリが使用している：MIDIアプリやUSBストレージアプリを閉じ、必要ならUSBを接続し直します。
- 途中で切断する：給電付きハブを試し、ファイルを再選択してBOOTSELからやり直します。
- 設定を維持したい：全消去用UF2やAdvancedの全消去は使いません。通常UF2には設定領域を含めていません。

## 参照

- [pico⚡flash開発元の対応環境と使い方](https://github.com/piersfinlayson/picoflash)
- [Raspberry Pi公式：BOOTSELとUF2](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html)
