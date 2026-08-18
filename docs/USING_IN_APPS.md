# Using Builder releases in apps

Do not fetch Wasm from GitHub Releases during a user's browser session. Pin a Builder tag in the consuming repository, download the matching profile ZIP during the app update/build process, verify SHA-256, then embed or vendor the assets into the app.

Examples:

```text
ffmpeg-wasm-video-compressor-v1.1.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.1.0.zip
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
