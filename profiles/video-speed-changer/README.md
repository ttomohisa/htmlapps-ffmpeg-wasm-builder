# video-speed-changer profile

Compact public-libav profile for Browser Kitty Video Speed Changer.

- Changes video timestamps with `setpts`.
- Changes audio tempo with `atempo` when preserving pitch.
- Uses `asetrate` + `aresample` when pitch should change together with speed.
- Can drop the audio stream entirely.
- Chains `atempo` stages so the public API accepts 0.25x to 4.00x.
- Autorotates display-matrix video before H.264 encoding.
- Outputs H.264 + AAC MP4, or H.264-only MP4 when audio is removed.
- Uses WORKERFS for browser `File` / `Blob` input.
- Does not expose arbitrary FFmpeg CLI arguments.

Build note: `buffer`, `buffersink`, `abuffer`, and `abuffersink` are public libavfilter graph endpoints that FFmpeg registers manually. They must not be passed through `--enable-filter=` or checked as `CONFIG_*_FILTER` components.

`v0.4.0` of the consuming app uses the full 0.25x to 4.00x range and all three audio modes through the same narrow public API.

## Threading variants (Builder 1.10.6)

`./build.sh video-speed-changer` (or `build-video-speed-changer.bat`) builds and real-browser smoke-tests both explicit variants:

- `dist/video-speed-changer/single-thread/`: no SharedArrayBuffer requirement; portable single-HTML/file:// support remains.
- `dist/video-speed-changer/multi-thread/`: SharedArrayBuffer and cross-origin isolation (COOP/COEP) required. Select it explicitly with `BrowserFFmpeg.loadEmbedded({ coreJsText, wasmBytes, threading: "multi-thread" })`.

The MT budget matches Compressor: 8 prewarmed pthread Workers, 2 video decoder threads, 4 H.264 encoder threads and 1 x264 lookahead thread. Audio codecs and both filter graphs remain single-threaded. Nested Workers reuse the generated `ffmpeg.js`; no separate pthread worker asset is needed. This is a bounded concurrency contract, not a guaranteed speedup for every input/device.

Tagged releases include `ffmpeg-wasm-video-speed-changer-single-thread-v1.10.6.zip` and `ffmpeg-wasm-video-speed-changer-multi-thread-v1.10.6.zip`, plus matching BUILDINFO. `ffmpeg-wasm-video-speed-changer-v1.10.6.zip` and `BUILDINFO-video-speed-changer.txt` remain byte-identical ST aliases. Both variants retain GPL-2.0-or-later notices and the matching corresponding-source archive.
