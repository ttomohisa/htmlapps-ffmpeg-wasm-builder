# Using Builder releases in apps

Do not fetch Wasm from GitHub Releases during a user's browser session. Pin a Builder tag in the consuming repository, download the matching profile ZIP during the app update/build process, verify SHA-256, then embed or vendor the assets into the app.

Examples:

```text
ffmpeg-wasm-video-compressor-v1.3.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.3.0.zip
ffmpeg-wasm-media-inspector-v1.3.0.zip
ffmpeg-wasm-video-contact-sheet-v1.3.0.zip
```

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
