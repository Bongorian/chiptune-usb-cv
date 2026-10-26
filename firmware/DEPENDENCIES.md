# ビルド依存関係

| 依存先 | 固定値 | ライセンス |
|---|---|---|
| Raspberry Pi Pico SDK | 2.3.1 / `079c6f39023649b154152db30f1d781e884879bc` | BSD-3-Clauseほか、SDKのLICENSEと各ソースを参照 |
| TinyUSB | SDKサブモジュール / `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` | MIT |
| picotool | `2041936441b48a3cc53ae3da9e805229fe8f4e18` | BSD-3-Clause |
| Arm GNU Toolchain | ローカル検証はGCC 14.3.0（Arduino RP2040 pqt-gcc 4.1.0-1aec55e） | GCC／newlibの各ライセンス |

SDKは`tools/bootstrap_sdk.sh`で公式リポジトリから取得します。TinyUSBとpicotoolも公式リポジトリから取得し、製品配布時には各依存先のライセンス条件を確認してください。
外部コードそのものはリポジトリに同梱していません。ビルド済みUF2／BINにはSDK、TinyUSB、Cランタイム由来のコードが含まれます。配布物に対応するライセンス・著作権通知は[`LICENSES/`](LICENSES/)に同梱しています。GCCのランタイムにはGPLとGCC Runtime Library Exceptionが適用されます。picotoolはビルド時の変換ツールです。

- [Pico SDK LICENSE](https://github.com/raspberrypi/pico-sdk/blob/079c6f39023649b154152db30f1d781e884879bc/LICENSE.TXT)
- [TinyUSB LICENSE](https://github.com/hathach/tinyusb/blob/86ad6e56c1700e85f1c5678607a762cfe3aa2f47/LICENSE)
- [picotool LICENSE](https://github.com/raspberrypi/picotool/blob/2041936441b48a3cc53ae3da9e805229fe8f4e18/LICENSE.TXT)

USB VID/PIDの既定値はTinyUSBの開発例で使われる値です。この値の製品用割り当てを受けたものではありません。
CMakeの`CHIPTUNE_USB_VID`／`CHIPTUNE_USB_PID`で変更できます。
