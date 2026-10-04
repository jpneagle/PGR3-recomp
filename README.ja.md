# PGR3-recomp

[English](README.md) | 日本語

Xbox 360 用ゲーム『Project Gotham Racing 3』を、静的再コンパイルで Windows ネイティブのプログラムに変換します。
ゲームの PowerPC コードを関数単位で C++ に変換してコンパイルし、Xbox 360 のカーネル・GPU・音声・入力は
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (Xenia 由来) のランタイムが受け持ちます。このリポジトリには、
その SDK に対する修正一式も含まれます。

このリポジトリにあるのは、プロジェクトファイル・ツール・SDK へのパッチだけです。**ゲームのコード・データ・素材は
一切含みません。** 変換は、ご自身が所有するゲームを使って、ご自身の PC 上で行います。

## 法的な注意と利用者の責任

- このプロジェクトは『Project Gotham Racing 3』のコード・データ・素材・スクリーンショットを含まず、配布もしません。
  本体の鍵やファームウェアも含みません。
- ゲームは、ご自身が正規に入手したものを用意してください。吸い出し・変換・実行が許されるかどうかは、お住まいの国の
  法律や同意した利用規約によって異なります。**それを確認し遵守することは、すべて利用者ご自身の責任です。**
- 変換で生成されるものは**配布しないでください**。生成された C++ (`generated\`)、ビルドした `pgr3.exe`、取り出した
  ゲームファイル (`titles\`、`assets`)、復号したイメージや解析結果は、いずれもゲームに由来するものです。
  ご自身の PC の中だけに留めてください。
- 本ソフトウェアは「現状のまま」提供され、いかなる保証もありません ([LICENSE](LICENSE) 参照)。**利用はすべて
  利用者ご自身の判断と責任で行ってください。** 本ソフトウェアの利用によって生じた損害、データの消失、法的な請求
  その他いかなる結果についても、作者は責任を負いません。
- これは独立した非営利の互換性・保存目的のプロジェクトです。Microsoft および Bizarre Creations とは無関係であり、
  許諾や承認を受けたものではありません。Project Gotham Racing、Xbox、Xbox 360 は各権利者の商標です。

## 対応バージョン: 日本語版のみ

現時点で対応しているのは **PGR3 の日本・アジア版だけ** です。

| 項目 | 値 |
|---|---|
| リリース | 日本・アジア (英・日・仏・独・西・伊・中・韓) |
| タイトル ID | `4D5307D1` |
| `default.xex` のビルド日時 | 2005-10-30 23:26:30 UTC |
| ディスクイメージ | XGD2 の `.iso` |

北米版・欧州版など他地域のものは**未確認**で、そのままでは動かない見込みです。`pgr3_config.toml` にある関数アドレスは
この実行ファイルに固有のもので、リポジトリ内の解析メモもすべてこの版を対象にしています。別の版に対応するには、
その部分をやり直す必要があります ([変換漏れ](#変換漏れ) を参照)。

ディスクに同梱の『Geometry Wars』(`gw.xex`) には対応していません。

## 動作状況

自動入力と、保存したフレーム画像・音声で確認できている範囲:

- 起動、オープニングムービー、メニュー、プロフィール作成、キャリア、ガレージ、車の購入、レース (ロンドン)
- 言語: ディスクに入っている 8 言語を起動時に選択
- 描画解像度は最大 4K (ゲームの 720p 出力を 2 倍または 3 倍で描画)
- BGM、エンジン音、曲名表示
- 車内視点を含むすべての視点、フォトモード
- Xbox コントローラー (SDL 経由)

既知の問題・未確認の点:

- 高解像度のとき、ポーズ画面のぼかした背景に細かい横縞が出る
- リプレイ用フォルダの作成に失敗する (`GAME:\Game\Replay`)。リプレイ保存は未確認
- オンライン対戦は使えない
- 実際に遊んで確認したのはゲームのごく一部。ほかにも不具合があるはずです

## 必要なもの

- Windows 10/11 (x64)、Direct3D 12 対応の GPU
- Visual Studio 2022 Build Tools (MSVC と Windows SDK。ヘッダーとライブラリに使用)
- MSVC ターゲット (`x86_64-pc-windows-msvc`) の LLVM/Clang 20 以降
- CMake 3.25 以降、Ninja、Git、Python 3
- 空きディスク容量 約 15 GB (ゲームファイル、SDK のビルド、生成コード)

## セットアップ

スクリプトは次の配置を前提にしています (環境変数で変更可能。`tools\env.bat` を参照)。

```
<root>\
  pgr3_recomp\        このリポジトリ
  ext\
    rexglue-sdk\      ReXGlue SDK (パッチを当ててビルドしたもの)
    llvm\             LLVM (bin\clang.exe など)
```

`tools\env.bat` がコンパイラの環境を整えます。既定の場所は `PGR3_LLVM` (LLVM のフォルダ)、`PGR3_TOOLS`
(`cmake\bin` と `ninja` を含むフォルダ)、`PGR3_VCVARS` (`vcvars64.bat` のパス) で上書きできます。

### 1. パッチを当てた SDK をビルドする

```bat
cd <root>\ext
git clone --recursive https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
git checkout c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git submodule update --init --recursive
git apply ..\..\pgr3_recomp\patches\rexglue-sdk.patch

..\..\pgr3_recomp\tools\env.bat cmake --preset win-amd64 ^
    -DCMAKE_CONFIGURATION_TYPES=Release;RelWithDebInfo -DCMAKE_DEFAULT_BUILD_TYPE=Release
..\..\pgr3_recomp\tools\env.bat cmake --build out/build/win-amd64 --config Release --target install
```

パッチは上記のコミットに対して作ったものです。Windows の Git が SDK のサブモジュール内のシンボリックリンクを
小さなテキストファイルとして取り出してしまう場合 (`libmspack` のビルドで失敗します) は、Git のシンボリックリンク
対応を有効にするか、それらのファイルをリンク先の実体のコピーに置き換えてください。

### 2. ゲームを取り出す

```bat
cd <root>\pgr3_recomp
python tools\xdvdfs.py "<お手持ちの PGR3 のディスクイメージ>.iso" extract titles\pgr3\game
```

### 3. 変換してビルドする

```bat
tools\build.bat
```

SDK のコード生成を `titles\pgr3\game\default.xex` に対して実行し (`generated\default\` に約 65 MB の C++)、
低優先度・2 ジョブでコンパイルします。できあがるのは `out\build\win-amd64-release\pgr3.exe` です。

## 遊び方

`play.bat` を起動すると、最初に設定画面が開きます。

- **ゲームフォルダ** — 取り出したディスクの中身 (`default.xex` があるフォルダ)
- **言語** — 日本語、英語、フランス語、ドイツ語、スペイン語、イタリア語、韓国語、繁体字中国語
- **解像度** — 720p、1080p、1440p、4K
- **フルスクリーン**

選んだ内容は exe と同じ場所の `pgr3_launcher.toml` に保存されます。「次回からこの画面を表示しない」にチェックすると
設定画面を飛ばします。Shift を押しながら起動すると再び表示されます。コマンドラインで指定したオプション
(`play.bat <ゲームフォルダ> --fullscreen=false ...`) が優先されます。

ログは `pgr3.log`、クラッシュ時の記録は `pgr3_crash.txt` に出ます。

## 仕組み

```
ISO --xdvdfs.py--> titles\pgr3\game\default.xex
                        |
     rexglue codegen (pgr3_manifest.toml + pgr3_config.toml)
                        v
              generated\default\*.cpp --Clang--> pgr3.exe + rexruntime.dll + rexgpu-xenos.dll
```

- **CPU**: PowerPC の関数 1 つが C++ の関数 1 つになります。ゲスト側のメモリと呼び出し規約は本体のままです。
- **カーネル / XAM**: SDK のランタイムがネイティブに実装しています。
- **GPU**: ゲームに組み込まれた Direct3D 9 ライブラリもそのまま再コンパイルします。それが本体の GPU 向けに
  書き出す命令列を、SDK の Xenos バックエンドが Direct3D 12 上で実行します。
- **このリポジトリのコード** (`src\`): アプリケーションの外枠、起動時の設定画面、クラッシュ記録、無人テスト用の仕組み。

### SDK パッチの内容

`patches\rexglue-sdk.patch` は、このゲームを動かす過程で見つけた修正をまとめたものです。

- `vupkd3d128` (64 ビットパック形式) の変換が誤っており、アニメーションのデータが壊れて車内視点が真っ黒になっていた
- XMP 音楽プレーヤー: WMA/MP3 のタイトルプレイリスト再生と `XMPCaptureOutput` が未実装だった
  (FFmpeg の libavformat をビルドに追加)。曲情報の構造体のレイアウト
- `XMASetLoopData` が引数を別の構造体として読んでおり、ループ再生の音が壊れていた
- `XGetLanguage` が常に英語を返していた
- ランタイムが作るスレッドで浮動小数点例外のマスクが外れたままになっていた
- デバッグ用の機能: 音声の書き出し、フレーム単位の描画コール記録、描画のスキップ、浮動小数点トラップ

### 変換漏れ

アンワインド情報を持たず、ポインタ経由でしか呼ばれない小さな関数は、SDK の解析で見落とされます。
`tools\find_entries.py` がデータ中の参照からそれらを探します。対応版の実行ファイルに対する結果は
`pgr3_config.toml` に入っています (アドレスのみ)。

### ツール

| ファイル | 用途 |
|---|---|
| `tools\xdvdfs.py` | Xbox / Xbox 360 のディスクイメージ内のファイル一覧・取り出し |
| `tools\xex.py` | XEX2 ヘッダーの表示、イメージの書き出し (鍵は同梱せず、`XEX_RETAIL_KEY` で渡す) |
| `tools\imports.py` | XEX のカーネル import と、SDK の実装状況 |
| `tools\find_entries.py` | コード生成が見落とした関数の先頭を探す |
| `tools\gfunc.py`、`maprip.py`、`gpudbg.py`、`audiodump.py`、`contact.py` | デバッグ用の補助 |
| `tools\run.sh`、`cockpit_test.sh` | 自動入力による無人実行 |

## ライセンスと謝辞

このリポジトリのファイルは [MIT ライセンス](LICENSE) で公開します。

`patches\rexglue-sdk.patch`、`generated\rexglue.cmake`、`src\` のアプリケーションのひな形は、BSD 3-Clause
ライセンスの ReXGlue SDK (Copyright (c) 2026 Tom Clay、一部 Copyright (c) Ben Vanik および Xenia プロジェクトの
コントリビューター) を改変したもの、またはそれに由来するものです。SDK 本体はこのリポジトリには含まれず、SDK 本体と
パッチを当てた部分には SDK のライセンスが適用されます。SDK は音声のデコードのために FFmpeg (LGPL) をビルドします。

このプロジェクトを可能にしてくれた ReXGlue SDK、Xenia、XenonRecomp の各プロジェクトに感謝します。
