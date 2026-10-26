# Chiptune Wave Lab

公開エディタ: **https://bongorian.github.io/chiptune-usb-cv/**

SAA1099／SN76489の実際の発音構造を編集する、依存ライブラリ不要の静的Webアプリです。GitHub PagesへActionsで公開します。

1. 基板へv0.9以降のUF2を書き込み、USBで接続。MIDI 10が初期配置です。
2. Chrome／EdgeなどWeb MIDI対応ブラウザーでエディタを開き、USB MIDI接続とSysExを許可。
3. Chiptuneの入出力を選択し「本体から読み出す」。
4. 音色を編集して「本体に反映」。基板のLINE OUTで試聴。
5. ステップ・共通クロック・CVは各反映ボタンで送信。保持する場合は「保存…」。

本体未接続でも音色編集とJSON書き出しができます。任意オーディオ波形のアップロードではなく、チップ内蔵ENV形状・トーン・ノイズ・レイヤーと時間変化の編集です。波形は模式図です。Web MIDIはHTTPS／localhostと対応ブラウザーが必要です。対応するAndroidのChromeでもUSBホスト接続して使用できます。ブラウザーと端末によるため、入出力ポートが見えることを確認してください。

## 初期配置

CH1–6=SAAの各1音、CH7–9=SNの各1音、CH10=SNノイズ。SNは左右へ同じ音程を送り、PAN/CC10でL/Rへ振ります。試聴の初期値はNOTE 45 / A2 / 110 Hz。単純なSAA TONEから編集を始めます。

HYBRIDの特殊発音はCH2=SAA ENV/AM、DUAL LAYERはCH1=SAA LAYERです。プロファイルを切り替えると全パートが置き換わります。v0.8への接続は維持しますが、MIDI 10の適用はv0.9以降が必要です。

## ローカル開発

リポジトリのルートから:

```sh
python3 firmware/tools/generate_editor.py
cmake -S firmware -B build/tests -G Ninja -DCHIPTUNE_HOST_TESTS=ON
cmake --build build/tests
build/tests/editor_export > editor/factory.json
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" node --test editor/tests/*.test.js
python3 -m http.server 8765 --directory editor
```

ブラウザー自動テストはPlaywrightとChromeを使う開発用テストです。通常のエディタにPlaywright等を配信しません。

```sh
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" node editor/tests/browser.cjs
```

`PLAYWRIGHT_MODULE`でモジュールパス、`CHROME_BIN`でChromeのパス、`EDITOR_URL`で対象URLを指定できます。実行中の利用者のブラウザーへは接続せず、独立したヘッドレスプロファイルでテストします。

[音色設計](../firmware/SOUND_DESIGN.md)／[通信仕様](../firmware/protocol/README.md)
