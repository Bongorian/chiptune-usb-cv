# Chiptune Play v0.10

演奏に主眼を置いた Web ページです。旧 Wave Lab（エディタ専用）は廃止し、鍵盤演奏を前面に出した構成に置き換えました。`editor/` はビルド不要の静的ファイルで、[公開版](https://bongorian.github.io/chiptune-usb-cv/) またはローカル HTTP サーバーで実行できます。

```sh
python3 -m http.server 8766 --directory editor
# Chrome等で http://localhost:8766
```

## 演奏する

- 画面の鍵盤は表示幅に合わせて音域が伸縮します（レスポンシブ）。マルチタッチ対応で、横へなぞるとグリッサンド、タッチ位置（上下）でベロシティが変わります。
- PC キーボード：A=C / W=C# / S=D / E=D# / D=E / F=F / T=F# / G=G / Y=G# / H=A / U=A# / J=B / K=次のC / O / L。Z / X でオクターブ移動、Space 長押しでサステイン。
- 未接続でもブラウザ内蔵音源で演奏できます。音源は SAA1099 / SN76489 の発音モデル（SQUARE 50%、PULSE 約6.7%、ENV WAVE、TONE+NOISE、NOISE、SN METAL、ADSR、ピッチ変化、グライド、ビブラート、トレモロ、レイヤー、左右デチューン）を Web Audio で近似した試聴用で、実音とは異なります。
- Track は画面上部のチップで切り替えます（全14 Track、初期配置は MIDI 10：ch1–6 SAA / ch7–9 SNトーン / ch10 SNノイズ）。工場音色23種は鍵盤の上のバーから即読み込みできます。
- 演奏モード DIRECT / POLY / LEGATO、POLY声数、音量、PAN は鍵盤の上の演奏コントロールで変更します。ブラウザ音源はモードに従います（DIRECT は後着優先で離鍵時に前の保持音へ復帰、LEGATO はエンベロープ継続）。

## 実機につなぐ

- Web MIDI 対応ブラウザ（Android / PC の Chrome 系）で USB 接続し、「実機に接続」から入出力を選びます。鍵盤のノート、ベンド、サステイン（CC64）、音量 / PAN（CC7 / CC10）は選択中 Track の受信 ch へ送ります（複数 ch のマスクでは最小の ch）。
- 音色の読み書きは従来どおり SysEx（protocol 2 / schema 10）で、firmware 0.10.0 の完全一致を確認した場合だけ本体書込を有効にします。旧ファームウェアには設定を送りません。
- iPhone の Safari は Web MIDI 非対応のため実機接続はできません。ブラウザ演奏は可能です。

## 音色設定（最新ファーム準拠）

- パラメータは `firmware/protocol/parameters.json` の全35項目（生成ファイル `schema.js` / `core.js` / `factory.json` は従来どおり C モデルから生成され、変更しません）。
- WAVE は SQUARE 50%（SAA/SN）、PULSE 約6.7%（SN）、ENV WAVE（SAA）、TONE + NOISE（SAA）を区別し、NOISE、SAA AM、SN METAL も選択できます。使えない設定は音源能力に従って表示しません（固定ノイズでの Pitch/Glide/Vibrato/Detune、SAA での White 切替など）。
- 「本体に反映」は RAM 編集です。保存は Patch / Track / Project / System から対象を選びます。「本体から読み出す」は本体の状態を取り込みます。LIVE は 100 ms で変更をまとめます。
- Sequence（16ステップ）、SN共通クロック、8変調ルート、CV/Gate、System は鍵盤の下の折り畳み欄にあります。
- 未接続でも 14 Track の音作りと v0.10 Track JSON の保存・読み込みができます。

## テスト

```sh
EDITOR_BRIDGE="$PWD/build/tests/editor_bridge" node --test editor/tests/*.test.js
```

旧 Wave Lab 用のブラウザテスト（`editor/tests/browser.cjs`）は新 UI では廃止しました。プロトコル契約テスト（core.test.js）はそのまま有効です。
