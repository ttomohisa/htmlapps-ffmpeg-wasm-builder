# FFmpeg WASM Builder

Build small, task-specific browser FFmpeg WebAssembly cores from pinned FFmpeg and Emscripten sources. Prebuilt `@ffmpeg/ffmpeg` / `@ffmpeg/core` binaries are not consumed.

The upstream `ffmpeg` CLI is not linked. Each profile enables only the FFmpeg components it needs and links a small runner against public `libav*` APIs. Pthreads are disabled, so the browser runtime needs neither SharedArrayBuffer nor cross-origin isolation.

## v1.1.0 profiles

- `video-compressor`: decode/filter/encode to H.264 + AAC MP4; links x264.
- `lossless-video-cutter`: packet-level stream copy with no decoder, encoder, filter, or x264 linked into the final Wasm. Blob/File input is exposed through Emscripten WORKERFS so the whole source file is not copied into MEMFS first.

Build with `build.bat <profile>` or `./build.sh <profile>`. Every release profile has a real headless-browser smoke test. The cutter smoke test performs an actual MP4 cut and validates that video/audio streams remain present.

The cutter exposes `BrowserFFmpeg.losslessVideoCutterArgs({ input, output, start, end, noAudio })`. Since inter-frame video must begin at a decodable keyframe, the actual start can be aligned to the keyframe at or before the requested time.

## Public releases

A v1.1.0 tag rebuilds and smoke-tests both profiles and publishes profile-specific binary ZIPs, build information, SHA-256 checksums, and one exact corresponding-source archive.

## Licensing

The root MIT license covers this repository's original builder/runtime source. It does **not** relicense generated `ffmpeg.wasm`. `video-compressor` is GPL-2.0-or-later because it enables GPL FFmpeg components and links x264. `lossless-video-cutter` enables no GPL-only component and is LGPL-2.1-or-later. See `THIRD_PARTY_NOTICES.md` and `docs/LICENSES.md`.
