# FFmpeg WASM Builder

FFmpeg + Emscripten から、用途ごとに小さく絞ったブラウザー向けFFmpeg WebAssemblyを**自前ビルド**するためのリポジトリです。`@ffmpeg/ffmpeg` / `@ffmpeg/core` の配布済みバイナリには依存しません。

FFmpeg本家CLIはリンクせず、各ツール専用のpublic `libav*` runnerだけをWASM化します。

```text
pinned FFmpeg / Emscripten / optional x264
                 ↓
          profile flags
                 ↓
        public libav* runner
                 ↓
          ffmpeg.js + ffmpeg.wasm
                 ↓
       real browser smoke test
```

- pthread不使用 / SharedArrayBuffer不要
- COOP / COEP不要
- Web Worker実行
- `file://` の単一HTMLでも利用可能
- `--disable-everything` からprofileごとに必要機能だけ有効化
- FFmpeg更新時は各profileを実ブラウザーでsmoke test
- Tagged Releaseでは対応ソース・build info・SHA-256を同時配布

初めて使う場合は [START-HERE.md](START-HERE.md) を先に読んでください。

## v1.1.0 profiles

| profile | 用途 | decoder / encoder | x264 | 主な出力 |
|---|---|---:|---:|---|
| `video-compressor` | 動画圧縮 | あり | あり | H.264 + AAC MP4 |
| `lossless-video-cutter` | 無劣化カット | **なし** | **なし** | 元codecのstream copy |

`lossless-video-cutter` はdecode / encode / filterをせず、圧縮済みpacketをそのままremuxします。開始位置は動画codecの性質上、直前のキーフレームへ調整される場合があります。入力File/BlobはWORKERFSでWorkerから必要な範囲だけ読み込むため、大きな入力を丸ごとMEMFSへ複製しません。出力は現在MEMFS上に作るため、切り出し結果そのもののサイズはブラウザーの利用可能メモリに依存します。

## pin

`versions.env` にまとめています。

- FFmpeg 9.0.1 / exact commit
- Emscripten 6.0.6 / exact source commit
- x264 stable / exact commit（x264を使うprofileだけが最終WASMへリンク）

## ビルド

Video Compressor:

```text
build.bat video-compressor
```

Lossless Video Cutter:

```text
build-lossless-video-cutter.bat
```

または：

```text
build.bat lossless-video-cutter
```

生成物はprofileごとに分かれます。

```text
dist/<profile>/
├─ ffmpeg.js
├─ ffmpeg.wasm
├─ ffmpeg.js.gz
├─ ffmpeg.wasm.gz
├─ manifest.json
└─ smoke-test.html
```

`build.bat` はWASM生成後にChrome / Edgeをheadless起動し、profile専用のsmoke testを実行します。

```text
[OK] Smoke test passed.
```

まで出れば、コンパイルだけでなくブラウザー上の実処理まで成功しています。

### Lossless Video Cutter のrunner API

```text
--input /workerfs/input.mp4
--output /output.mp4
--start 13.4
--end 102.8
--no-audio   # optional
```

Browser runtimeでは：

```js
const inputPath = "/workerfs/input.mp4";
const args = BrowserFFmpeg.losslessVideoCutterArgs({
  input: inputPath,
  output: "/output.mp4",
  start: 13.4,
  end: 102.8
});

await runner.run({
  files: [{ name: inputPath, data: file, workerfs: true }],
  outputs: ["/output.mp4"],
  args
});
```

出力拡張子でcontainerを選びます。profileにはMP4/MOV、Matroska/WebMのdemux/muxを含めています。codec自体は再エンコードしないため、選んだcontainerが元codecを受け入れられる必要があります。

## アプリへ組み込む場合

ブラウザー実行時にGitHub Releaseへアクセスするのではなく、**アプリの更新・ビルド時に特定Builder Releaseを取得し、そのアプリへ固定して埋め込む**方式を推奨します。

詳細は [docs/USING_IN_APPS.md](docs/USING_IN_APPS.md) を参照してください。

## Public Release

v1.1.0ではRelease workflowが両profileをbuild + smoke testしてから次を公開します。

```text
ffmpeg-wasm-video-compressor-v1.1.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.1.0.zip
ffmpeg-wasm-sources-v1.1.0.tar.gz
BUILDINFO-video-compressor.txt
BUILDINFO-lossless-video-cutter.txt
SHA256SUMS.txt
```

詳しくは [docs/RELEASING.md](docs/RELEASING.md) を参照してください。

## FFmpeg更新

```text
check-updates.bat
```

候補を確認し、`versions.env` を1 dependencyずつ更新します。mainのGitHub Actionsでは現在の全release profileを実ブラウザーで確認するため、「ビルドできたが処理できない」更新を検出できます。

## 次のツールを追加する

[docs/ADDING_PROFILE.md](docs/ADDING_PROFILE.md) を参照してください。profile metadata、FFmpeg flags、runner、profile smoke testを追加する構成です。

## ライセンス

**ルートのMIT LicenseはBuilder自身のソースに対するものです。生成された `ffmpeg.wasm` をMITとして配布するものではありません。**

`video-compressor` は `--enable-gpl` + libx264 のため生成coreをGPL-2.0-or-laterとして扱います。`lossless-video-cutter` はGPL-only componentやx264を使わないため、生成coreはLGPL-2.1-or-laterです。ルートMITはBuilder自身のコードに適用され、生成coreを再ライセンスするものではありません。

- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [docs/LICENSES.md](docs/LICENSES.md)
