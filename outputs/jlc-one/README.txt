JLCONE提出用パッケージ — CHIPTUNE USB/CV 4層版

外側のまとめZIPは保管・受け渡し用です。展開後、upload/内の3点を
JLCONEの対応する入力欄にそれぞれ指定してください。
1. 基板データ: upload/Chiptune_Gerber.zip（このZIPは展開せず提出）
2. 部品表: upload/Chiptune_BOM.csv
3. 実装座標: upload/Chiptune_CPL.csv

表面実装141点、BOM 41行、LCSC部品40種類。
独自KiCad名ではなく、LCSCの保存済みカタログにあるメーカー型番と
パッケージ名を使用。交換品C20526/C2286のBasic区分と在庫を2026-10-05に確認。
残りの全品目の最新在庫・発注総額は未確認です。
reference/Assembly_Details.csvで回路上の用途とメーカー部品を照合できます。

USB-C J1(C165948)はJLCPCB公開在庫から調達・表面実装する対象です。
My Partsに使用可能在庫があれば利用し、新規調達時はJLCPCB公開在庫から同じC165948を選ぶ。
BOM/CPLには調達元の指定欄がないため、ファイルだけで切り替わるとは限りません。
1基板につき1個。注文画面の実装対象にJ1が選択されていることを確認する。

発注設定: 85×54 mm、角R3、FR-4 1.6 mm、4層、表面1面実装、緑レジスト。
Economic PCBAで見積もり、部品の対応可否・費用を画面で確認してください。
数量は希望する枚数を画面で設定してください。パネルではなく単体基板のデータです。
詳しい製造条件: reference/Order_Specification.txt

手実装: SAA1099＋SN76489AN×2とDIPソケット、OLEDモジュールと接続、
任意のSWDヘッダー。BOOT/RESETは通常開放の背面サービスパッドです。
reference/Hand_Assembly.csvのLCSC欄は別途購入用で、PCBA対象ではありません。
配置確認図: reference/Assembly_Top.svg（表面）、Assembly_Bottom.svg（裏面反転表示）。
配置確認図はFabの参照図で、重複ラベルを非表示にした閲覧用コピーから出力。
基板の部品座標・向きは原本と同一で、基板に印刷されるシルクではありません。
Routing_Overview.pngは配線閲覧図です。製造データには使用しないでください。
KiCad_Routed_Project.zipは配線済み原本とライブラリの保存用です。

確認済み: DRC・未接続・ERC・回路図一致は各0件。BOM/CPLの全141点一致、
座標範囲、重複参照番号、ZIP CRC、4銅箔層＋PTH/NPTHを照合済み。
checks/Package_Verification.jsonおよびSHA256SUMS.txtに検証結果とハッシュを保存。
アップロード後に必要: 実装プレビューで1番ピン・極性・回転・USB/ジャック開口方向
を確認。回転・中心補正はライブラリのパッド番号と位置決め穴に基づく補正候補。
ライブのJLCONEモデル上での一致は未確認です。過去の手動調整を重ねず、
修正版BOM/CPLを新規に読み込んでPlacement_Pin1.svgと照合してください。
全IC・トランジスタ・ダイオードとジャックの照合記録: Placement_Audit.csv。
今回の部品変更と費用比較: Basic_Parts_Review.csv、Assembly_Review.txt。
FB1は10Ω抵抗(C22859)でR4と同じ部品のため、同一部品複数行の警告が出る
可能性があります。回路上のFB1という参照番号は維持しています。
ファームウェアと実機試験（電流・突入、音声、CV校正）は未完了です。
このパッケージでアップロード・発注は行っていません。

公式案内:
https://jlcpcb.com/help/article/jlcone-desktop-app
https://jlcpcb.com/help/article/advice-for-bom-and-cpl-files-preparation
