# Wave Lab v0.10

v0.10の構成可能なPatch/Trackを編集するWeb MIDIエディタです。`editor/`はビルド不要の静的ファイルです。この開発ブランチを公開サイトへ配信する操作は行っていません。

```sh
python3 -m http.server 8766 --directory editor
# Chrome等で http://localhost:8766
```

未接続でも14 Trackの音作りとv0.10 Track JSONの保存・読み込みができます。接続にはWeb MIDI対応ブラウザーとSysEx許可が必要です。MIDIの入力・出力を選択し、firmware 0.10.0 / protocol 2 / schema 10が一致した場合だけ本体書込を有効にします。旧ファームウェアには設定を送りません。

通常画面では演奏モード、音量、PAN、音源、ADSR、ピッチ変化、グライド、ビブラート等を操作します。AdvancedではENVの形状、回路構造、層、音域、個別USB/TRSチャンネル、マクロ、Morphを編集します。機能の表示は音源能力に従い、使用回路の最大数と本体が割り当てた出力／専用クロックを表示します。ADSR図は模式図です。

- 「本体に反映」はRAM編集。保存はPatch/Track/Project/Systemから対象を選びます。
- 「本体から読み出す」は本体の状態を取り込み、現在のローカル編集を置き換えます。
- 接続時に未反映編集がある場合は保持します。接続しただけでは保存しません。
- LIVEは100 msで変更をまとめ、最新値を順次送信します。応答時にドラッグ中のスライダーを置き換えません。競合・タイムアウト後に同じ失敗を無限再送しません。
- 双方向同期は明示読出しと書込応答です。本体操作の変更を常時プッシュ受信する仕組みはありません。
- POLY FOCUSは同じチップのほかのTrackをOFFにして声数を集めます。別チップは保持します。テンプレートとProject読込は全配置を置き換えます。
- モーフはUSER Patchとの同構造補間です。異なるチップ／Source／ENV形状等は本体が拒否します。
- Sequence、共通SNクロック、8変調ルート、CV/Gate、Systemは折り畳み欄にあります。それぞれRAM反映してからProjectまたはSystemを保存します。

プロトコルは[仕様](../firmware/protocol/README.md)、全体操作と制約は[本体README](../firmware/README.md)を参照してください。工場音色はCの同じモデルから生成し、パラメータ定義は`firmware/protocol/parameters.json`から共有します。

```sh
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" node --test editor/tests/*.test.js
# PlaywrightとChrome/Chromiumを用意し、上記サーバーを起動した状態で:
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" EDITOR_URL=http://127.0.0.1:8766 CHROME_BIN=/path/to/chrome node editor/tests/browser.cjs
```

ブラウザーテストのMIDIはCファームウェアに接続したモックです。実機への送信や書込は行いません。
