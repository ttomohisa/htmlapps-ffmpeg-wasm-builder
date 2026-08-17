# FFmpeg WASM Builder

FFmpeg + x264 + Emscripten から、ブラウザー向けの小さなFFmpeg WebAssemblyを**自前ビルド**するためのリポジトリです。`@ffmpeg/ffmpeg` / `@ffmpeg/core` の配布済みバイナリには依存しません。

現在は構成を一本化し、**FFmpeg本家CLIは使わず、公開 `libav*` APIの独自runnerだけ**をビルドします。

```text
FFmpeg / x264 / Emscripten
          ↓
  public libav* libraries
          ↓
  profile-specific runner
          ↓
   ffmpeg.js + ffmpeg.wasm
```

- FFmpeg更新へ追従しやすい
- pthread不使用
- SharedArrayBuffer不要
- COOP / COEP不要
- Web Workerで処理
- `file://` の単一HTMLでも利用可能
- ビルド後に**実動画変換のsmoke testを自動実行**
- タグReleaseでは**対応ソースとライセンスを同時配布**

初めて使う場合は [START-HERE.md](START-HERE.md) を先に読んでください。

## v1.0.0 のpin

`versions.env` にすべて集約しています。

- FFmpeg 9.0.1 / exact commit
- Emscripten 6.0.6 / exact source commit
- x264 stable / exact commit

更新候補は `check-updates.bat` で確認できます。FFmpeg / Emscripten / x264を一度に全部変えず、1つずつ更新してsmoke testを通してください。

## ビルド

Docker Desktopを起動してから：

```text
build.bat
```

生成物：

```text
dist/video-compressor/
├─ ffmpeg.js
├─ ffmpeg.wasm
├─ ffmpeg.js.gz
├─ ffmpeg.wasm.gz
├─ manifest.json
└─ smoke-test.html
```

`build.bat` はWASMを生成しただけでは成功扱いにしません。生成後、Microsoft Edge / Chromeなどをheadless起動し、`tests/fixtures/smoke-input.mp4` を実際にH.264 + AAC MP4へ変換します。出力に `ftyp` / `moov` / `mdat` があり、H.264 (`avc1`) + AAC (`mp4a`) のMP4になっているところまで確認できた場合だけ成功します。

```text
[OK] Smoke test passed.
```

まで出れば、ビルドだけでなくブラウザー上の実変換まで成功しています。

## 単一HTML

```text
pack-single-html.bat
```

生成：

```text
dist/single-html-video-compressor.html
```

JS/WASMはgzipのままHTMLに埋め込み、初回変換時に展開します。SharedArrayBufferや特殊なHTTPヘッダーは不要で、ダブルクリックの `file://` 起動にも対応します。`demo.bat` はこの単一HTMLを生成して開きます。

## アプリへ組み込む場合

公開アプリがブラウザー実行時にGitHubへWASMを取りに行く構成は推奨しません。**アプリの更新・ビルド時にこのBuilderの特定Releaseを取得し、自分の配布物へ固定して組み込む**のを基本方針にしています。

```text
FFmpeg WASM Builder の version tag
        ↓ build + smoke test
GitHub Release
        ↓ appの更新時に取得
htmlapps-video-compressor
        ↓ 単一HTMLへ埋め込み
公開後はGitHubへ依存しない
```

詳細は [docs/USING_IN_APPS.md](docs/USING_IN_APPS.md) を参照してください。

## Public Release

`main` をpushしただけではReleaseは作りません。`main` のBuild FFmpeg WASMが成功したことを確認してから、`BUILDER_VERSION` と同じタグをpushします。

```text
git tag -a v1.0.0 -m "FFmpeg WASM Builder v1.0.0"
git push origin v1.0.0
```

タグworkflowは再ビルドとsmoke testを通した後、次を自動作成します。

```text
ffmpeg-wasm-video-compressor-v1.0.0.zip
ffmpeg-wasm-sources-v1.0.0.tar.gz
BUILDINFO.txt
SHA256SUMS.txt
```

詳しい手順は [docs/RELEASING.md](docs/RELEASING.md) を参照してください。

## FFmpeg更新時

```text
check-updates.bat
```

候補を確認して `versions.env` の1項目だけ変更し、`build.bat` を実行します。configure・Cコンパイル・WASMリンク・ブラウザー実変換のどこかが壊れれば、その時点で失敗します。

## 次のアプリを追加する

[docs/ADDING_PROFILE.md](docs/ADDING_PROFILE.md) にprofile追加手順をまとめています。

## ライセンス

**ルートのMIT LicenseはBuilder自身のソースに対するものです。生成された `ffmpeg.wasm` をMITとして配布するものではありません。**

`video-compressor` profileはlibx264をリンクするためFFmpegを `--enable-gpl` でビルドします。生成されるFFmpeg/x264 Wasm coreはGPL-2.0-or-laterの条件に従って配布する必要があります。タグReleaseでは、exact source・ビルドレシピ・upstream license filesを同じReleaseへ添付します。

- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [docs/LICENSES.md](docs/LICENSES.md)

ライセンス説明は実装・配布のための技術的整理であり、法的助言ではありません。
