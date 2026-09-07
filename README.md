# FFmpeg WASM Builder

FFmpeg + Emscripten から、用途ごとに小さく絞ったブラウザー向けFFmpeg WebAssemblyを**自前ビルド**するためのリポジトリです。`@ffmpeg/ffmpeg` / `@ffmpeg/core` の配布済みバイナリには依存しません。

FFmpeg本家CLIはリンクせず、各ツール専用のpublic `libav*` runnerだけをWASM化します。

```text
pinned FFmpeg / Emscripten / optional x264 / libvpx / Opus / libwebp
                 ↓
          profile flags
                 ↓
        public libav* runner
                 ↓
          ffmpeg.js + ffmpeg.wasm
                 ↓
       real browser smoke test
```

- 既存profileは従来どおりsingle-thread / SharedArrayBuffer不要
- `ffmpeg-filter-builder` はsingle-thread + multi-threadを同時生成
- single-thread版はCOOP / COEP不要で`file://`単一HTML向け
- multi-thread版はSharedArrayBuffer + cross-origin isolation（COOP / COEP）が必要
- どちらもWeb Worker上で実行
- `--disable-everything` からprofileごとに必要機能だけ有効化
- FFmpeg更新時は各profileを実ブラウザーでsmoke test
- Tagged Releaseでは対応ソース・build info・SHA-256を同時配布

初めて使う場合は [START-HERE.md](START-HERE.md) を先に読んでください。

## v1.9.4 profiles

| profile | 用途 | decoder / encoder | x264 | 主な出力 |
|---|---|---:|---:|---|
| `video-compressor` | 動画圧縮 | あり | あり | H.264 + AAC MP4 / VP9 + Opus WebM |
| `video-speed-changer` | 動画速度変更 | あり | あり | H.264 + AAC MP4 / H.264-only MP4 |
| `lossless-video-cutter` | 無劣化カット | **なし** | **なし** | 元codecのstream copy |
| `media-inspector` | codec / fps / bitrate / metadata解析 | **なし** | **なし** | structured JSON report |
| `video-contact-sheet` | 動画全体から12/24/48枚を均等抽出 | decoderのみ | **なし** | RGB PPM + sample JSON |
| `video-to-gif` | 動画の一部をAnimated GIF化 | decode + GIF encode | **なし** | GIF89a animation |
| `video-to-webp` | 動画の一部をAnimated WebP化 | decode + libwebp_anim | **なし** | Animated WebP |
| `ffmpeg-filter-builder` | Visual filtergraph preview/render | decode/filter/encode | あり | H.264 + AAC MP4（ST / MT） |


`video-compressor` はv1.6.0で H.264/AAC MP4 に加えて VP9/Opus WebM を出力できます。入力動画はWORKERFSで読み込み、圧縮前のinspect modeでは映像packetの総bytesとstream durationから平均映像bitrateを測定します。MP4/MOVのDisplay Matrixも読み取り、90/180/270度の回転はFFmpeg本体と同じtranspose/flip分岐で実画素へ適用してからresize/encodeします。

`lossless-video-cutter` はdecode / encode / filterをせず、圧縮済みpacketをそのままremuxします。開始位置は動画codecの性質上、直前のキーフレームへ調整される場合があります。入力File/BlobはWORKERFSでWorkerから必要な範囲だけ読み込むため、大きな入力を丸ごとMEMFSへ複製しません。出力は現在MEMFS上に作るため、切り出し結果そのもののサイズはブラウザーの利用可能メモリに依存します。

`media-inspector` はフレームをdecodeせず、container / stream headerとmetadataを解析してJSONを返します。MP4/MOV/M4A、MKV/WebM、AVI、MPEG-TS/PS、FLV、ASF、Ogg、MP3、WAV、FLAC、AAC、AC-3/E-AC-3などを対象に、codec、解像度、FPS、pixel format、profile/level、bitrate、HDR、音声、字幕、chapter、rotationを取得します。入力はWORKERFSを使うため、大きなFile/Blobを丸ごとMEMFSへ複製しません。

`video-contact-sheet` は動画全体の指定地点へseekし、12 / 24 / 48枚のフレームだけをdecodeして1枚のRGB PPMへ並べます。H.264 / HEVC(H.265) / VP8 / VP9 / AV1 / MPEG-4 / MJPEG / ProResなどを対象に、Pixelの`hvc1`系HEVCもブラウザーのネイティブ再生可否に依存せずFFmpeg WASMでdecodeします。画像encoderはリンクせず、アプリ側CanvasでPNG/JPEG保存する前提です。

`video-to-gif` は短い動画区間をtrim / FPS削減 / autorotate / resizeし、1回目のdecodeで `palettegen`、2回目で `paletteuse` を行う2-pass構成です。GIF encoderはFFmpeg内蔵のみを使い、x264/libwebp/audio stackはリンクしません。

`video-to-gif` / `video-to-webp` はv1.5.0から、autorotate後の映像に対する正規化crop矩形 (`x/y/width/height`: 0..1) を受け取れます。crop後にresizeするため、アプリ側はプレビュー上の切り抜き枠をそのままrunnerへ渡せます。

`video-to-webp` は同じ動画前処理を共有し、FFmpegの `libwebp_anim` wrapperからAnimated WebPを生成します。libwebpはこのprofileだけにリンクされ、lossy/lossless、quality、compression levelを選べます。両profileとも入力File/BlobはWORKERFSを使います。

`ffmpeg-filter-builder` は同一profileから `single-thread` と `multi-thread` の2 runtimeを生成します。MT初期設定はpthread pool 8に対して、video decoder 2 / libx264 encoder 4 / x264 lookahead 1を明示的に割り当てます。decodeとencodeを同じ4-thread設定にせず、ブラウザの有限なWorker poolを枯渇させない構成です。ST版は可搬性優先、MT版はBrowser Kitty等のcross-origin isolatedなHTTP(S)配信向けです。初期v0.1 runnerは1入力のvideo/audio filter chainを実行し、complex multi-input graphはFilter Builderアプリの後続段階で拡張します。

## pin

`versions.env` にまとめています。

- FFmpeg 9.0.1 / exact commit
- Emscripten 6.0.6 / exact source commit
- x264 stable / exact commit（x264を使うprofileだけが最終WASMへリンク）
- libwebp 1.6.0 / exact commit（`video-to-webp`だけが最終WASMへリンク）
- libvpx 1.16.0 / exact commit（`video-compressor`のVP9出力だけが最終WASMへリンク）
- Opus 1.5.2 / exact commit（`video-compressor`のWebM音声出力だけが最終WASMへリンク）

## ビルド

Video Compressor:

```text
build.bat video-compressor
```

Video Speed Changer:

```text
build-video-speed-changer.bat
```

または：

```text
build.bat video-speed-changer
```

Lossless Video Cutter:

```text
build-lossless-video-cutter.bat
```

または：

```text
build.bat lossless-video-cutter
```

Media Inspector:

```text
build-media-inspector.bat
```

または：

```text
build.bat media-inspector
```

Video Contact Sheet:

```text
build-video-contact-sheet.bat
```

または：

```text
build.bat video-contact-sheet
```

Video to Animated GIF:

```text
build-video-to-gif.bat
```

Video to Animated WebP:

```text
build-video-to-webp.bat
```

FFmpeg Filter Builder（ST + MTを連続build）:

```text
build-ffmpeg-filter-builder.bat
```

または：

```text
build.bat ffmpeg-filter-builder
```

生成物はprofileごとに分かれます。通常profileは従来のflat配置を維持し、dual-runtime profileだけvariant subdirectoryを使います。

```text
dist/<profile>/
├─ ffmpeg.js
├─ ffmpeg.wasm
├─ ffmpeg.js.gz
├─ ffmpeg.wasm.gz
├─ manifest.json
└─ smoke-test.html

dist/ffmpeg-filter-builder/
├─ single-thread/
│  └─ ffmpeg.js / ffmpeg.wasm / manifest.json / smoke-test.html ...
└─ multi-thread/
   └─ ffmpeg.js / ffmpeg.wasm / manifest.json / smoke-test.html ...
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


### Media Inspector のrunner API

```text
--input /workerfs/input.mp4
--output /report.json
```

Browser runtimeでは：

```js
const inputPath = "/workerfs/input.mp4";
const reportPath = "/report.json";
const result = await runner.run({
  files: [{ name: inputPath, data: file, workerfs: true }],
  outputs: [reportPath],
  args: BrowserFFmpeg.mediaInspectorArgs({
    input: inputPath,
    output: reportPath
  })
});
const report = BrowserFFmpeg.decodeJsonOutput(result, reportPath);
```

JSONにはcontainer、duration、総bitrate、各streamのcodec / tag / profile / level、videoの解像度 / FPS / pixel format / HDR / rotation、audioのsample rate / channel layout / bitrate、metadata、chapter、subtitle情報を含めます。

「この動画がなぜブラウザーで再生できないか」の判定は、WASMへブラウザー固有ルールを埋め込まず、アプリ側でこのJSONと `HTMLMediaElement.canPlayType()` / `MediaCapabilities` を組み合わせる方針です。これにより **Media Doctor** 的な診断へ発展させやすくしています。

### Video Contact Sheet のrunner API

```text
--input /workerfs/input.mp4
--output /contact-sheet.ppm
--metadata-output /contact-sheet.json
--count 24
--thumb-size 320
```

Browser runtimeでは：

```js
const inputPath = "/workerfs/input.mp4";
const ppmPath = "/contact-sheet.ppm";
const jsonPath = "/contact-sheet.json";
const result = await runner.run({
  files: [{ name: inputPath, data: file, workerfs: true }],
  outputs: [ppmPath, jsonPath],
  args: BrowserFFmpeg.videoContactSheetArgs({
    input: inputPath,
    output: ppmPath,
    metadataOutput: jsonPath,
    count: 24,
    thumbSize: 320
  })
});
const ppm = BrowserFFmpeg.decodePpmOutput(result, ppmPath);
const meta = BrowserFFmpeg.decodeJsonOutput(result, jsonPath);
```

PPMはCanvasへ描画してPNG/JPEGへ保存します。runnerは各地点の直前キーフレームへseekして必要なところまでだけdecodeするため、長時間動画のcontact sheet生成で全フレームを最初から最後までdecodeする必要はありません。

### Video to Animated GIF のrunner API

```text
--input /workerfs/input.mp4
--output /output.gif
--start 3.2
--end 8.0
--max-width 480
--fps 15
--colors 128
--dither sierra2_4a
```

Browser runtimeでは：

```js
const result = await runner.run({
  files: [{ name: "/workerfs/input.mp4", data: file, workerfs: true }],
  outputs: ["/output.gif"],
  args: BrowserFFmpeg.videoToGifArgs({
    start: 3.2, end: 8.0, maxWidth: 480, fps: 15,
    colors: 128, dither: "sierra2_4a",
    crop: { x: 0.125, y: 0, width: 0.75, height: 1 }
  })
});
```

### Video to Animated WebP のrunner API

```text
--input /workerfs/input.mp4
--output /output.webp
--start 3.2
--end 8.0
--max-width 480
--fps 15
--quality 75
--compression-level 4
--lossless 0
```

Browser runtimeでは `BrowserFFmpeg.videoToWebpArgs()` を使います。`lossless: true` を指定するとBGRA経路のlossless WebP、それ以外はYUV420Pのlossy WebPを生成します。

## FFmpeg Filter Builder runner API

初期v0.1 profileは、Graph compilerが生成した1-stream filter chainをpublic libav runnerへ渡します。

```js
const args = BrowserFFmpeg.ffmpegFilterBuilderArgs({
  input: "/workerfs/input.mp4",
  output: "/output.mp4",
  videoFilter: "crop=1080:1080,scale=720:720",
  audioFilter: "volume=-3dB"
});
```

MT版をembedded assetから起動するときは `threading: "multi-thread"` を指定します。Emscripten 6系では `ffmpeg.js` 自身をpthread Workerとして再利用し、runtimeが `mainScriptUrlOrBlob` に埋め込みJSのBlobを渡します。cross-origin isolationがない環境では明示的に失敗します。ST版は従来どおり`file://`利用を維持します。

## アプリへ組み込む場合

ブラウザー実行時にGitHub Releaseへアクセスするのではなく、**アプリの更新・ビルド時に特定Builder Releaseを取得し、そのアプリへ固定して埋め込む**方式を推奨します。

詳細は [docs/USING_IN_APPS.md](docs/USING_IN_APPS.md) を参照してください。

## Public Release

v1.9.4では既存7 profileに加え、FFmpeg Filter BuilderのST/MTをbuild + smoke testして公開します。

```text
ffmpeg-wasm-video-compressor-v1.9.4.zip
ffmpeg-wasm-video-speed-changer-v1.9.4.zip
ffmpeg-wasm-lossless-video-cutter-v1.9.4.zip
ffmpeg-wasm-media-inspector-v1.9.4.zip
ffmpeg-wasm-video-contact-sheet-v1.9.4.zip
ffmpeg-wasm-video-to-gif-v1.9.4.zip
ffmpeg-wasm-video-to-webp-v1.9.4.zip
ffmpeg-wasm-ffmpeg-filter-builder-single-thread-v1.9.4.zip
ffmpeg-wasm-ffmpeg-filter-builder-multi-thread-v1.9.4.zip
ffmpeg-wasm-sources-v1.9.4.tar.gz
BUILDINFO-ffmpeg-filter-builder-single-thread.txt
BUILDINFO-ffmpeg-filter-builder-multi-thread.txt
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

`video-compressor` と `video-speed-changer` は `--enable-gpl` + libx264 のため生成coreをGPL-2.0-or-laterとして扱います。`lossless-video-cutter`、`media-inspector`、`video-contact-sheet`、`video-to-gif`、`video-to-webp` はGPL-only componentやx264を使わないため、生成coreはLGPL-2.1-or-laterです。`video-to-webp` がリンクするlibwebpは別途upstream noticeをRelease bundleへ同梱します。ルートMITはBuilder自身のコードに適用され、生成coreを再ライセンスするものではありません。

- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- [docs/LICENSES.md](docs/LICENSES.md)


## Video Speed Changer

`video-speed-changer` provides a compact H.264/AAC speed-change core using public libav APIs, WORKERFS input, `setpts` for video timing, chained `atempo` for pitch-preserving audio, `asetrate` + `aresample` for pitch-shifting audio, and optional audio removal. Browser apps should call `BrowserFFmpeg.videoSpeedChangerArgs(...)`.
