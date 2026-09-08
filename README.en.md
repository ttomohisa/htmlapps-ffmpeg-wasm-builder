# FFmpeg WASM Builder

Build small, task-specific browser FFmpeg WebAssembly cores from pinned FFmpeg and Emscripten sources. Prebuilt `@ffmpeg/ffmpeg` / `@ffmpeg/core` binaries are not consumed, and the upstream `ffmpeg` CLI is not linked. Each profile uses a public `libav*` runner.

## v1.9.5 profiles

Existing profiles remain single-threaded and keep their previous `dist/<profile>/` contract: `video-compressor`, `video-speed-changer`, `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, and `video-to-webp`.

`ffmpeg-filter-builder` is the first dual-runtime profile. Its initial MT contract prewarms an 8-worker pthread pool and limits codec work to 4 threads. One build produces:

- `dist/ffmpeg-filter-builder/single-thread/`: no SharedArrayBuffer or COOP/COEP requirement; intended for portable offline/single-HTML use including `file://`.
- `dist/ffmpeg-filter-builder/multi-thread/`: pthread-enabled FFmpeg/x264/Emscripten output that reuses `ffmpeg.js` as the pthread Worker program; requires SharedArrayBuffer and cross-origin isolation.

Build it with `build-ffmpeg-filter-builder.bat`, `build.bat ffmpeg-filter-builder`, or `./build.sh ffmpeg-filter-builder`. The build runs the ST smoke test with the existing portable path and the MT smoke test through a local COOP/COEP HTTP server.

The Filter Builder runner exposes `BrowserFFmpeg.ffmpegFilterBuilderArgs(...)` and accepts compiled `--video-filter` / `--audio-filter` chains. v1.9.5 adds the video `trim` filter plus runner-level `--start-time` / `--duration` range rendering. The runner automatically pairs video `trim + setpts` with audio `atrim + asetpts`; those audio/timestamp companion filters, together with `volume` and `aresample`, were already enabled in v1.9.4. It intentionally does not claim full multi-input `filter_complex` support yet; that remains a later Filter Builder application milestone.

For embedded MT use, `BrowserFFmpeg.loadEmbedded(...)` accepts `threading: "multi-thread"`. Emscripten 6.x reuses the generated main JS as its pthread Worker program, so the runtime passes the Blob-hosted `ffmpeg.js` through `mainScriptUrlOrBlob` and refuses MT startup when cross-origin isolation is unavailable.

## Public releases

A v1.9.5 tag rebuilds and smoke-tests the existing seven ST profiles plus both FFmpeg Filter Builder variants. The release publishes separate `ffmpeg-wasm-ffmpeg-filter-builder-single-thread-v1.9.5.zip` and `ffmpeg-wasm-ffmpeg-filter-builder-multi-thread-v1.9.5.zip` assets, BUILDINFO files, SHA-256 checksums, and one exact corresponding-source archive.

## Licensing

The root MIT license covers this repository's original builder/runtime source. It does **not** relicense generated `ffmpeg.wasm`. Profiles that enable GPL FFmpeg components and link x264, including `ffmpeg-filter-builder`, produce GPL-2.0-or-later cores. See `THIRD_PARTY_NOTICES.md` and `docs/LICENSES.md`.
