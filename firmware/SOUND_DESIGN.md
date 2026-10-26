# v0.11 音作り

音色は固定の構造番号を選ぶ方式から、Chip / Source / ENV / Layersの組合せへ変更しました。すべて同じPatchと発音エンジンを使います。[操作と制約](README.md)も参照してください。

1. 初期のSAA TONEかSN MONOで開始し、ADSR、ピッチ変化、グライド、LFOを調整します。PAN中央でSNは同じノートを左右へ送ります。左右差が必要な音色だけdetuneを増やします。
2. SAAのWAVEはSQUARE 50% / ENV WAVE / TONE + NOISE / NOISE / SAA AMから選びます。AdvancedではSourceとENVを個別に設定できます。OscillatorはENV自体を波形として使い、AMはTone/Noiseを変調します。8形状と8/16段、右反転が使えます。Zero/Maximumは周期音ではありません。
3. SAAでLayers=2にするとENV＋補助Tone/Noise/Mixになります。Layer interval/level/decayでアタックの打音やオクターブ層を作れます。追加減衰は全体ADSRへ掛けるもので、独立した第2 ADSRではありません。
4. SNのWAVEはSQUARE 50% / PULSE 約6.7% / NOISE / SN METALから選びます。PULSEはPeriodic Noise＋Tone 3追従で、周期の約1/15のパルスを出します。Tone 3はクロック専用として消音します。SQUAREでLayers=2/3にすると音程差を重ねます。Stereoは4/6トーン回路を使います。NoiseのKey TrackはTone 3を予約し、MixはそのTone 3も出力します。固定ノイズには連続ピッチやスイープを付けません。
5. 同じ構造の2音色をUSERへ保存するとMorphで連続パラメータを補間できます。形状・音源・回路構造の切替はモーフ対象外です。MacroやCV/LFOをMorphへ割り当てることもできます。

工場音色は23種類です。特殊音色専用MIDI chはなく、載せたTrackの受信chで弾きます。資源不足ならPOLY FOCUS、声数の削減、別TrackのOFFを明示的に行います。通常の音色読込は声数を勝手に減らしません。

SAAの3声ごとの共有ノイズ、ENVクロック、SNのTone 3依存と共通クロックは物理制約です。Wave Labの使用回路表示と拒否理由を確認してください。通常SAA MixのToneとNoiseの連続混合率はハードウェアにありません。SN Metalは別のTone/Noise音量を持つのでmixが使えます。ENV補助レイヤーがノイズクロックを兼ねる場合は、その補助Toneとノイズの速度が結び付きます。

音色ができたらPatch保存。PANや演奏モードも再利用するならTrack保存、ルート・パターンごとならProject保存です。CV校正と明るさはSystem保存です。
