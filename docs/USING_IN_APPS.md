# Using Builder releases in apps

Do not fetch Wasm from GitHub Releases during a user's browser session. Pin a Builder tag in the consuming repository, download the matching profile ZIP during the app update/build process, verify SHA-256, then embed or vendor the assets into the app.

Examples:

```text
ffmpeg-wasm-video-compressor-v1.6.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.6.0.zip
ffmpeg-wasm-media-inspector-v1.6.0.zip
ffmpeg-wasm-video-contact-sheet-v1.6.0.zip
ffmpeg-wasm-video-to-gif-v1.6.0.zip
ffmpeg-wasm-video-to-webp-v1.6.0.zip
```


## Video Compressor

Builder v1.6.0 adds VP9/Opus and inspection to the existing compact H.264/AAC profile. Keep the browser `File` on WORKERFS for both inspection and compression.

```js
const input = "/workerfs/input.mp4";
const reportPath = "/inspect.json";
const inspected = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [reportPath],
  args: BrowserFFmpeg.videoCompressorInspectArgs({ input, output: reportPath })
});
const report = BrowserFFmpeg.decodeJsonOutput(inspected, reportPath);

const output = "/output.webm";
const compressed = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [output],
  args: BrowserFFmpeg.videoCompressorArgs({
    input, output, codec: "vp9", videoBitrate: report.video.bitRateKbps
  })
});
```

The inspection bitrate is calculated from actual video-stream packet bytes and stream duration. Display-matrix rotation is applied to pixels before resizing/encoding.

The profile bundle contains `ffmpeg.js`, `ffmpeg.wasm`, gzip copies, `manifest.json`, `browser-ffmpeg.js`, `BUILDINFO.txt`, and license notices.

For Lossless Video Cutter, use:

```js
BrowserFFmpeg.losslessVideoCutterArgs({
  input: "/input.mp4",
  output: "/output.mp4",
  start: 13.4,
  end: 102.8,
  noAudio: false
})
```

The operation is a stream copy. The actual start may be moved backward to the nearest decodable keyframe. Surface that behavior in the app UI rather than promising frame-accurate cuts without re-encoding.

## Lossless Video Cutter and large input files

The cutter profile is built with Emscripten WORKERFS. Keep the browser `File`/`Blob` intact and pass it to the runtime as `{ name: "/workerfs/input.mp4", data: file, workerfs: true }`. Do not call `arrayBuffer()` on a large input first. The current runtime still writes the cut result to MEMFS before returning it.


## Media Inspector

Use a browser `File`/`Blob` as WORKERFS input and request the small JSON report as a MEMFS output:

```js
const input = "/workerfs/input.bin";
const output = "/report.json";
const result = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [output],
  args: BrowserFFmpeg.mediaInspectorArgs({ input, output })
});
const report = BrowserFFmpeg.decodeJsonOutput(result, output);
```

The Wasm report contains deterministic media facts. Browser playback advice should be derived in the app layer with APIs such as `HTMLMediaElement.canPlayType()` and MediaCapabilities instead of hard-coding browser support policy into the Wasm core.


## Video Contact Sheet

Use WORKERFS for the source video and request the PPM plus optional JSON metadata:

```js
const input = "/workerfs/input.mp4";
const ppmPath = "/contact-sheet.ppm";
const jsonPath = "/contact-sheet.json";
const result = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [ppmPath, jsonPath],
  args: BrowserFFmpeg.videoContactSheetArgs({
    input,
    output: ppmPath,
    metadataOutput: jsonPath,
    count: 24,
    thumbSize: 320
  })
});
const ppm = BrowserFFmpeg.decodePpmOutput(result, ppmPath);
const meta = BrowserFFmpeg.decodeJsonOutput(result, jsonPath);
```

The PPM contains RGB24 pixels only. Draw it into Canvas, add timestamp labels if desired, then export PNG/JPEG with browser-native APIs. The Wasm core performs the seek/decode step itself, including HEVC/H.265 sources such as Pixel `hvc1` recordings.


## Video to Animated GIF

Keep the source `File`/`Blob` on WORKERFS and return only the generated GIF through MEMFS:

```js
const input = "/workerfs/input.mp4";
const output = "/output.gif";
const result = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [output],
  args: BrowserFFmpeg.videoToGifArgs({
    input, output,
    start: 2.5,
    end: 7.5,
    maxWidth: 480,
    fps: 15,
    colors: 128,
    dither: "sierra2_4a",
    crop: { x: 0.125, y: 0, width: 0.75, height: 1 }
  })
});
```

The runner performs two decode passes over only the selected time range: the first builds the palette, the second applies it and writes the animation.

## Video to Animated WebP

```js
const input = "/workerfs/input.mp4";
const output = "/output.webp";
const result = await runner.run({
  files: [{ name: input, data: file, workerfs: true }],
  outputs: [output],
  args: BrowserFFmpeg.videoToWebpArgs({
    input, output,
    start: 2.5,
    end: 7.5,
    maxWidth: 480,
    fps: 15,
    quality: 75,
    compressionLevel: 4,
    lossless: false,
    crop: { x: 0, y: 0.125, width: 1, height: 0.75 }
  })
});
```

The WebP profile alone links libwebp. Apps that ship both GIF and WebP can embed both gzip cores and only inflate/load the one selected by the user.

## Animation crop rectangles

`videoToGifArgs()` and `videoToWebpArgs()` accept `crop: { x, y, width, height }`. Crop coordinates are normalized to `0..1`, refer to the autorotated source frame, must stay inside that frame, and are applied before the final resize.
