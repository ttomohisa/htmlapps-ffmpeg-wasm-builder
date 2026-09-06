# FFmpeg WASM Builder

Build small, task-specific browser FFmpeg WebAssembly cores from pinned FFmpeg and Emscripten sources. Prebuilt `@ffmpeg/ffmpeg` / `@ffmpeg/core` binaries are not consumed.

The upstream `ffmpeg` CLI is not linked. Each profile enables only the FFmpeg components it needs and links a small runner against public `libav*` APIs. Pthreads are disabled, so the browser runtime needs neither SharedArrayBuffer nor cross-origin isolation.

## v1.8.1 profiles

- `video-compressor`: decode/filter/encode to H.264 + AAC MP4 or VP9 + Opus WebM; links x264, libvpx, and Opus. It measures source video bitrate from actual demuxed video-packet bytes, uses WORKERFS input, and applies display-matrix autorotation before resize/encode.
- `video-speed-changer`: H.264/AAC MP4 speed change with WORKERFS, display-matrix autorotation, `setpts`, pitch-preserving `atempo`, pitch-shifting `asetrate` + `aresample`, and optional audio removal; links x264.
- `lossless-video-cutter`: packet-level stream copy with no decoder, encoder, filter, or x264 linked into the final Wasm. Blob/File input is exposed through Emscripten WORKERFS so the whole source file is not copied into MEMFS first.
- `media-inspector`: structured JSON inspection of container, codec, FPS, bitrate, metadata, chapters, subtitles, rotation, audio layout, and HDR signals without decoding frames. WORKERFS keeps large input files out of MEMFS.
- `video-contact-sheet`: seek-based 12/24/48-frame sampling, video decode, rotation-aware RGB scaling, and one PPM contact sheet plus optional JSON timestamps.
- `video-to-gif`: trim/autorotate/fps/scale plus a two-pass FFmpeg `palettegen` + `paletteuse` GIF encoder path.
- `video-to-webp`: the same video preprocessing with FFmpeg `libwebp_anim`; pinned libwebp is linked only for this profile.

Build with `build.bat <profile>` or `./build.sh <profile>`. For Video Speed Changer on Windows, `build-video-speed-changer.bat` is the direct entry point. Every release profile has a real headless-browser smoke test. The cutter smoke test performs an actual MP4 cut and validates that video/audio streams remain present; the inspector validates the structured H.264/AAC report; the contact-sheet smoke test seeks/decodes 12 samples and validates the generated RGB PPM + JSON metadata.

The cutter exposes `BrowserFFmpeg.losslessVideoCutterArgs({ input, output, start, end, noAudio })`. Since inter-frame video must begin at a decodable keyframe, the actual start can be aligned to the keyframe at or before the requested time.


Media Inspector exposes `BrowserFFmpeg.mediaInspectorArgs({ input, output })`; the returned `/report.json` can be parsed with `BrowserFFmpeg.decodeJsonOutput(result, "/report.json")`. Browser-specific playback diagnosis should live in the app layer by combining the report with `HTMLMediaElement.canPlayType()` / `MediaCapabilities`, keeping the Wasm core deterministic and reusable.

Video Contact Sheet exposes `BrowserFFmpeg.videoContactSheetArgs({ input, output, metadataOutput, count, thumbSize, columns })`. Parse the generated P6 image with `BrowserFFmpeg.decodePpmOutput(result, path)`, then use Canvas for PNG/JPEG export. HEVC is decoded inside FFmpeg Wasm, so Pixel `hvc1` recordings do not depend on native browser HEVC playback support.

## Public releases

A v1.8.1 tag rebuilds and smoke-tests all seven profiles and publishes profile-specific binary ZIPs, build information, SHA-256 checksums, and one exact corresponding-source archive.

## Licensing

The root MIT license covers this repository's original builder/runtime source. It does **not** relicense generated `ffmpeg.wasm`. `video-compressor` and `video-speed-changer` are GPL-2.0-or-later because they enable GPL FFmpeg components and link x264. `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, and `video-to-webp` enable no GPL-only FFmpeg component and are LGPL-2.1-or-later. The WebP bundle also carries libwebp notices. The video-compressor bundle additionally carries libvpx and Opus notices/patent grants. See `THIRD_PARTY_NOTICES.md` and `docs/LICENSES.md`.


## Video Speed Changer

`video-speed-changer` provides a compact H.264/AAC speed-change core using public libav APIs, WORKERFS input, `setpts` for video timing, chained `atempo` for pitch-preserving audio, `asetrate` + `aresample` for pitch-shifting audio, and optional audio removal. Browser apps should call `BrowserFFmpeg.videoSpeedChangerArgs(...)`.
