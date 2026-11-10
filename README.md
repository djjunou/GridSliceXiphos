<p align="center">
  <img src="assets/logo.png" alt="GridSliceXiphos" width="720">
</p>

# GridSliceXiphos — BPMグリッド・スライサー (VST3 / AU / Standalone)

**v1.2.0 Beta 1**

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
  - 同梱スキン（プラグインに内蔵、いずれも作者提供のクロームロゴアート）:
    `Chrome Bloom (Default)`＝水色クロームの花 / `Chrome Sky`＝青×ピンク＋ディスコボール /
    `Iridescent`＝ティール系イリディセント
  - `Img Opacity` で画像の見え具合を調整、`None` で画像なし
  - 選択したスキンはプロジェクトに保存されます
  - スキン差し替えは `assets/skins/` のPNGを同名で置き換えて再ビルド（コード変更不要）

## 変更履歴

### v1.2.0 Beta 1 (2026-11-11)
- バージョン表記に Beta 表記を導入（GUIタイトル横に「Beta 1」表示）。今回から Beta 1
- スキンを全面差し替え: 作者提供のクロームロゴアート3枚（デフォルト＝Chrome Bloom、ほか Chrome Sky / Iridescent）
- 旧スキン4枚（剣テーマ）は削除
- GUIを NEO Y2K / MONTAGEM 風に全面リスタイル: クロームグラデーション＋グロー付きタイトル、
  ネオングラデーション（シアン→紫→マゼンタ）のスライダー・コンボ・ボタン・トグル、
  クロームつまみ、波形もシアンのグロー＋マゼンタのグリッド線に統一

### v1.1.0 (2026-11-11)
- Windows + FL Studio 実機で v1.0 の動作を確認（BPM140・1/16 で128スライス等）
- PNG選択機能（Set PNG Image / Custom PNG）を削除（実機で正常に動作しなかったため、本人指示）
- 文字化け修正: タイトル等の UI 文字列から全角ダッシュを排除し、MSVC に `/utf-8` を追加

### v1.0.0 (2026-11-10)
- 初版: BPMグリッド切り、MIDI再生、フィルター、スライス書き出し、スキン4枚＋カスタムPNG

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
