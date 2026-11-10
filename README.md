# GridSliceXiphos — BPMグリッド・スライサー (VST3 / AU / Standalone)

Slicex の不満点ひとつを解消するために作った、軽量なサンプルスライサーです。

> **Slicex は BPM を指定して等間隔に切れない。**
> GridSliceXiphos は **BPM と「何分音符か」を指定して、ファイルの頭から等間隔に全切り**します。
> 例: BPM 180・16分音符 → 1スライス = 44.1kHz なら **3675 サンプル** ぴったり。

設計の参考にした [INTERSECT](https://github.com/tucktuckg00se/INTERSECT)（GPL-3.0）にも
「既存スライスの等分」はあっても全体のグリッド切りはなかったため、新規実装しています。
本プロジェクトも GPL-3.0 で公開します。

---

## 機能 (v1.0)

- **BPMグリッド切り**: BPM 30–300（手入力）× 分割 1/1, 1/2, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/64
  - 境界はスライスごとに理想位置から丸めるので、割り切れないBPMでも**誤差が累積しません**
  - `Sync to Host` を ON にすると FL のプロジェクトテンポに追従（既定は OFF = サンプル側のBPMで切る）
  - `Offset ms` で切り始め位置をずらせます
- **MIDI再生 (Slicex式)**: スライス *i* は ノート `Root Note + i`（既定 Root = 36）で発音
  - One-Shot（スライス終端まで再生）/ Gate（ノートオフで止まる）切替
  - 波形クリックでスライス試聴、再生中スライスのハイライト表示
- **フィルター内蔵**: Off / LowPass / HighPass / BandPass + Cutoff + Resonance (Q)
  フィルターをかけた音をそのままキー音にできます
- **デクリック**: Fade 0–20ms（アタック/リリースに適用）
- **書き出し**:
  - スライスを 24bit WAV で一括書き出し（`name_001.wav` …）
  - `bms/` サブフォルダに BMS 用36進ファイル名（`01.wav`, `0A.wav` …）でも同時書き出し
  - `_map.txt` にスライス一覧と、そのまま貼れる `#WAV01 …` 定義行を出力
- **状態保存**: パラメータ＋サンプルのパスを FL プロジェクトに保存
- **GUIスキン**: 背景画像を切り替え可能
  - 同梱スキン（プラグインに内蔵）: `Xiphos (Default)`＝岩に立つジフォス直剣 /
    `Bronze Blade`＝斜めの青銅剣 / `Spartan Dawn`＝夕焼けの重装歩兵 /
    `Marble Temple`＝大理石の神殿柱
  - `Custom PNG`: **自分の PNG（JPGも可）を背景にセット可能**。`Set PNG Image...` から選択
  - `Img Opacity` で画像の見え具合を調整、`None` で画像なし
  - 選択したスキンとカスタムPNGのパスはプロジェクトに保存されます

対応フォーマット: WAV / AIFF / FLAC / MP3 / OGG（JUCE標準）

---

## FL Studio での使い方

1. `GridSliceXiphos.vst3` を `C:\Program Files\Common Files\VST3\` に置く
2. FL の Plugin Manager でスキャン → Channel rack に GridSliceXiphos を追加
3. プラグイン画面でサンプルをドラッグ&ドロップ（または Load Sample）
4. **BPM と Division を設定**（例: 180 / 1/16）→ 波形にグリッド線と番号が出ます
5. ピアノロールで Root Note（既定36）から半音階にノートを置けば、各スライスが鳴ります

### BMS 音切りワークフロー (Mid2BMS blue mode)

1. フィルター等をかけたい音源を FL に用意し、GridSliceXiphos に読み込む
2. BPM/Division を曲に合わせて設定し、ピアノロールにノートを並べてパターンを作る
3. そのパターンを Mid2BMS に渡せる形で書き出し（MIDI + レンダリングWAV）、blue mode で音切り
   - スライス境界は等間隔グリッドで決まっているので、ノート位置と音の対応がずれません
4. プラグインから直接切りたい場合は **Export Slices** で `bms/` フォルダ＋`_map.txt` の
   `#WAV` 定義がそのまま使えます

---

## ビルド方法

### Windows（推奨: GitHub Actions）

このフォルダを自分の GitHub リポジトリに push すると、付属の
`.github/workflows/build.yml` が Windows / macOS / Linux 版を自動ビルドします。
完了後、Actions の Artifacts から `GridSliceXiphos-windows-latest` をダウンロードしてください。

### Windows（ローカル）

Visual Studio 2022（C++ワークロード）と CMake が必要です。

```bat
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

成果物: `build\GridSliceXiphos_artefacts\Release\VST3\GridSliceXiphos.vst3`

> **注意: MinGW ではビルドできません。** JUCE 8 が MinGW サポートを終了しているため
> （`Support for MinGW has been removed`）、Windows ビルドは MSVC（上記2方式のいずれか）が必須です。
> Linux からの MinGW クロスコンパイルは 2026-11-10 に実地で確認済み（不可）。

### Linux（この開発環境で検証済み）

```bash
sudo apt install libasound2-dev libx11-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype6-dev \
  libfontconfig1-dev libgtk-3-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure   # スライス計算の単体テスト
```

初回 configure 時に JUCE 8.0.4 を自動取得します（ネットワーク必須）。

---

## 既知の制限 (v1)

- 読み込めるサンプルは1つだけ（Slicex/INTERSECT のような複数セッションなし）
- タイムストレッチなし（グリッドで切るだけ。ピッチ変更もなし）
- サンプルの状態保存は「ファイルパス」のみ。ファイルを移動すると再リンクが必要
- プリセット/スライス個別パラメータなし（フィルター等は全体共通）

## ライセンス

GPL-3.0（JUCE フレームワークの GPL ライセンス形態でビルド。
参考プロジェクト INTERSECT も GPL-3.0）
