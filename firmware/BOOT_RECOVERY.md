# v0.8.1 起動修正と復旧

[chiptune-v0.8.1-alpha.uf2](dist/chiptune-v0.8.1-alpha.uf2)は専用RP2040基板用です。

## 更新手順

1. USBを抜きます。
2. 本体の **−と＋を押しながらUSBを接続**し、`RPI-RP2`が見えたらボタンを離します。
3. 上記のUF2を`RPI-RP2`へコピーします。ドライブが消えたら、USBを抜いて、ボタンを押さずに接続し直します。
4. MacのAudio MIDI設定などでUSB MIDIとして認識することと、OLEDの表示をそれぞれ確認します。`SYSTEM > VERSION`は`V0.8.1 ALPHA`です。
5. MIDI ch1にノートを送り、右下のJ3（LINE OUT）から音が出ることを確認します。

ファームウェアのボタン操作で`RPI-RP2`に入れない場合は、電源を外し、基板のJP1（BOOT）を短絡してからUSBを接続します。ドライブが見えたら短絡を外します。JP2はRESETです。

通常の更新でフラッシュ全消去は不要です。UF2は設定領域を含みません。起動時も設定領域を書き換えません。

## 保存設定を使わずに起動する

v0.8.1では **OKだけを押しながらUSBを接続**すると、保存された設定を読み飛ばして初期設定で起動します。接続後はOKを離してください。最初の数秒は`DEFAULTS (RAM)`と表示します。

これはRAM内の初期化です。電源を抜き、ボタンを押さずに接続すると元の保存内容を再び読み込みます。この起動方法では保存済みのCV校正・明るさも使いません。元の校正を残す場合は、この状態でSAVEせず、通常起動へ戻してください。

−と＋の同時押しは従来どおりUF2更新用のBOOTSELです。OKだけの初期設定起動とは別の操作です。

## 修正した原因

v0.8.0は設定用に末尾64KiBを予約するため、`pico_override_flash_size(chiptune 2031616)`を使用していました。このSDK関数は、リンカーのアプリ用範囲に加え、フラッシュドライバーの`PICO_FLASH_SIZE_BYTES`も`0x1f0000`へ変更します。

一方、設定の保存先は`0x1f0000`と`0x1f8000`の32KiBバンクです。どちらもSDKに伝わった容量を越えるため、`flash_range_erase` / `flash_range_program`の`hard_assert`で停止します。Releaseビルドにもこのチェックは残ります。初回の設定移行がUSB/OLED初期化前に自動保存していたため、画面とUSB MIDIの両方が起動しませんでした。

v0.8.1では次のように変更しました。

- SDKへは実物のフラッシュ容量 **2MiB** を伝え、リンカーだけでアプリの末尾64KiB使用を禁止します。容量の混同はコンパイル時にも検出します。
- 起動時の自動保存を廃止します。旧設定はRAMへ移行し、明示的なSAVE時だけ別バンクへ保存します。
- USBを設定読み込みより先に初期化し、初期化の合間にもUSBタスクを実行します。
- 設定読み込みの失敗は初期値へフォールバックし、停止するassertを使いません。
- CRCが正しい設定だけを意味検証し、旧4KiB領域のヘッダーが大きい形式を偽装しても領域外を読みません。
- OKを押しながら接続する、設定を消さない初期設定起動を追加しました。

有効なv0.8の保存設定は読み込みます。v0.7以前の演奏設定は新しい配置へRAM内で初期化し、CV校正と明るさは移行します。保存前の再起動では同じRAM移行を再度行います。

## 検証範囲

ASan/UBSan付きのPCテスト10件に加え、配布用ELFのCortex-M0+命令をUnicornで実行しました。

- 空のフラッシュ、有効／破損したv6、v5の校正付きバイナリ、OKを押した起動を検査。
- USB初期化 → USBタスク → 設定読み込み → エンジン/UI/OLED初期化 → USBタスクの順序を検査。起動中のerase/program呼び出しを禁止。
- SDKの実際のARMフラッシュ関数で、両バンクの全体書き込み・コミットページ・最終ページの容量チェック通過と、物理容量外の拒否を検査。
- 元のv0.8.0を同じ環境で再ビルドし、同じ保存先が容量チェックで拒否されることを再現。
- UF2のRP2040識別子、USB MIDI記述子、設定領域と重ならないこと、PIOクロックを検査。

エミュレーションではSDKのハードウェア初期化、USBタスク、SIOの整数除算、ROMのメモリー操作と浮動小数点比較を置き換えています。**実機のUSB列挙、OLED表示、フラッシュ保持、音声出力は未確認**です。検証結果は[Boot_Verification.json](dist/Boot_Verification.json)、[Verification.json](dist/Verification.json)、[Test_Verification.json](dist/Test_Verification.json)に記録します。

```sh
python3 -m pip install -r firmware/tests/requirements-arm.txt
python3 firmware/tests/check_arm_boot.py build/firmware/chiptune.elf --output build/firmware/Boot_Verification.json
```
