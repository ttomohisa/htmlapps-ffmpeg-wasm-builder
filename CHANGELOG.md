# Changelog

## 1.9.6 - 2026-09-08

- Fixed bounded Filter Builder renders on long inputs: `trim` / `atrim` can complete the filter graph before the one-second demux guard, and the resulting filter-source `AVERROR_EOF` is now treated as successful stream completion instead of exit code 1.
- Fixed dimension-changing Filter Builder video filters being silently scaled back to the source geometry. The runner now probes the caller video chain before opening the encoder and uses the negotiated sink dimensions for H.264 output.
- Bumped the Filter Builder public-libav runner to 0.2.1.
- Strengthened ST/MT smoke coverage so the scale filter is verified without `maxWidth` / `maxHeight` masking and a very short bounded range exercises early filter completion.

## 1.9.5 - 2026-09-08

- Fixed the repository check to validate the Filter Builder range API against `runtime/browser-ffmpeg.js` via the existing `$runtime` path variable; this prevents StrictMode CI failure from an undefined `$browserRuntime` variable.

- Added the FFmpeg `trim` video filter to the `ffmpeg-filter-builder` ST/MT profile. The companion timeline/audio filters (`setpts`, `atrim`, `asetpts`, `volume`, and `aresample`) were already present in v1.9.4 and remain enabled.
- Extended the Filter Builder public-libav runner to accept `--start-time` and `--duration`, automatically applying bounded video/audio timeline trimming and resetting output timestamps so short previews are actually bounded instead of transcoding the complete source.
- Added an early decode stop with a one-second reorder guard after the requested range, while keeping the filter graph responsible for exact output bounds.
- Extended `BrowserFFmpeg.ffmpegFilterBuilderArgs()` with `startTimeSeconds` / `durationSeconds` and advertised `timeRangeRender` in the runtime manifest capabilities.
- Expanded the real ST/MT browser smoke test to render a 0.5-second H.264/AAC range, inspect the produced MP4, and verify its duration and audio stream.
- Restored the `PacketMeasure` helper type used by `--inspect-output`; the first v1.9.5 candidate accidentally dropped this typedef while refactoring progress/range handling, causing the runner C compile to fail before linking.

## 1.9.4 - 2026-09-07

- Fixed the FFmpeg Filter Builder multi-thread runtime worker budget so the H.264 decoder and libx264 encoder no longer contend for the same four-thread allocation.
- Split the MT codec budget into 2 decoder threads and 4 encoder threads, while keeping the prewarmed pthread pool at 8 workers.
- Capped x264 lookahead to one worker and forced frame-based x264 threading for predictable browser behavior.
- Added encoder-open diagnostics that report the thread budget and the concrete libav error code.

## 1.9.3 - 2026-09-07

- Fixed the Filter Builder multi-thread output contract for Emscripten 6.x. Non-ESM pthread builds reuse the generated `ffmpeg.js` as the pthread Worker program instead of emitting a separate `ffmpeg.worker.js`.
- Removed the obsolete worker-file requirement from Docker/PowerShell/Unix build validation, smoke-test packaging, release packaging, and browser runtime loading.
- Embedded MT startup now passes the same `ffmpeg.js` source as a Blob through Emscripten `mainScriptUrlOrBlob`, preserving the single-HTML design without a second Worker asset.
- Added manifest metadata (`pthreadWorkerStrategy: main-script-url-or-blob`) and repository assertions so the Emscripten 6.x worker contract is explicit.

## 1.9.2 - 2026-09-07

- Fixed smoke-test packaging for the dual-thread Filter Builder profile. `__THREADING_MODE__` is embedded inside a JavaScript source line, so the packer now replaces scalar placeholders in-place instead of requiring the placeholder to occupy the whole line.
- Kept the unresolved-placeholder guard intact and added a repository assertion so this packaging regression cannot silently return.

## 1.9.1 - 2026-09-07

- Fixed the new `ffmpeg-filter-builder` profile so PNG decoding can actually be configured with FFmpeg 9.0.1. The profile now explicitly enables FFmpeg zlib support and Emscripten's zlib system port instead of declaring `png` without its `inflate_wrapper` dependency.
- Added `PROFILE_USE_ZLIB` as an opt-in profile capability. Existing profiles keep zlib disabled and therefore keep their previous dependency/size contract.
- Pass `-sUSE_ZLIB=1` consistently through FFmpeg configure/build and the final public-libav runner link for both single-thread and multi-thread Filter Builder variants.
- Added configure/repository assertions for `CONFIG_ZLIB` and recorded zlib linkage in the runtime manifest.

## 1.9.0 - 2026-09-07

- Added the `ffmpeg-filter-builder` public-libav profile with real video/audio filter-chain execution and H.264/AAC MP4 output for the Filter Builder v0.1 runtime PoC.
- Added explicit profile threading variants. Existing profiles remain single-threaded; Filter Builder builds both `single-thread` and `multi-thread` artifacts from the same source.
- Added pthread-aware FFmpeg/x264/Emscripten build paths with an 8-worker pool / 4 codec threads, generated pthread worker packaging, manifest schema 8 threading metadata, and embedded-browser runtime support via `mainScriptUrlOrBlob`.
- Added a COOP/COEP local smoke server for the multi-thread variant while preserving the existing portable ST smoke path.
- Updated CI and release packaging to publish separate Filter Builder ST/MT ZIPs and BUILDINFO files.

## 1.8.1 - 2026-09-06

- Add AbortSignal support to browser runtime runs so apps can terminate an active FFmpeg Worker cleanly.
- Ensure `dispose()` terminates and rejects any active run instead of leaving a Worker alive.
- Add `videoSpeedChangerInspectArgs()` for source metadata fallback without changing the video-speed-changer runner or WASM payload.
- Keep the video-speed-changer runner at 1.8.0; this patch changes only the browser runtime/package contract.
- Document release-consumer pinning by exact tag, profile asset, and SHA-256 so app release builds are reproducible without runtime network access.

## 1.8.0 - 2026-09-06

- Extend `video-speed-changer` to all three audio modes: preserve pitch, shift pitch with speed, or remove audio.
- Add the `asetrate` filter and `--pitch-shift` runner option while keeping existing `--no-audio` behavior.
- Extend `BrowserFFmpeg.videoSpeedChangerArgs()` with `preservePitch: false` and keep `noAudio: true` backward compatible.
- Expand the real-browser smoke test to cover every 0.25x to 4.0x preset plus pitch-shifting and silent output.

## 1.7.2 - 2026-09-05

- Fixed the Video Speed Changer profile so libavfilter buffer source/sink endpoints are no longer passed to `--enable-filter=` or asserted as nonexistent `CONFIG_*_FILTER` components.
- Preserved the input video time base in the H.264 encoder to avoid PTS collapse at fractional and faster playback rates.
- Skip `setpts` / `atempo` at exactly 1.0x so the baseline transcode follows the proven compressor filter path.
- Kept rate-specific browser smoke diagnostics with recent FFmpeg logs.

## 1.7.0

- Add `video-speed-changer` profile with H.264/AAC MP4 output.
- Add `BrowserFFmpeg.videoSpeedChangerArgs()` helper.
- Add real-browser speed-change smoke test for 1.0x, 1.5x, and 2.0x.


## 1.6.0 - 2026-09-03

- Fixed release packaging under `set -u` by defaulting profile-specific libvpx/Opus flags to disabled when older profiles do not define them.
- Fixed the FFmpeg configure component name for VP9: `--enable-encoder=libvpx_vp9`; the runtime codec name remains `libvpx-vp9`.
- Added a repository check so the configure/runtime naming distinction cannot regress.
- Added VP9/WebM output to `video-compressor` with a pinned encoder-only libvpx 1.16.0 build.
- VP9 speed modes now use 0 / 8 / 16 / 25 frames of look-ahead so slower modes trade more memory and time for better compression efficiency while the default stays moderate.
- Added Opus audio for WebM with pinned Opus 1.5.2 and kept H.264/AAC MP4 as the default path.
- Added `--inspect-output` to measure actual selected video/audio stream packet bytes, duration, FPS, display dimensions, and average stream bitrate without decoding all frames.
- Added Display Matrix autorotation using the same 90/180/270-degree transpose/flip decisions as FFmpeg so rotated MP4/MOV output is physically oriented correctly.
- Separated ffprobe-style display rotation reporting from FFmpeg autorotate's internal normalized angle so a 90-degree Display Matrix is reported as 90 while the filter path correctly uses its 270-degree autorotate equivalent.
- Enabled WORKERFS for video-compressor input so inspection and compression can read browser File/Blob data without first copying the full source into MEMFS.
- Added dependency-aware Docker stages and release packaging for libvpx/Opus while leaving every unrelated profile unchanged.
- Extended the browser runtime and smoke test to cover H.264, VP9, source inspection, and WORKERFS input.


## 1.5.0 - 2026-08-20

- Added normalized positioned crop rectangles to `video-to-gif` and `video-to-webp`.
- `BrowserFFmpeg.videoToGifArgs()` / `videoToWebpArgs()` now accept `crop: { x, y, width, height }`.
- Crop is applied after autorotation and before the final Lanczos resize.

## 1.4.0

- Added separate `video-to-gif` and `video-to-webp` profiles for short video clips -> animated images.
- GIF profile uses a two-pass `palettegen` -> `paletteuse` runner with configurable colors/dithering, trim, FPS reduction, autorotation, scaling, and infinite looping.
- WebP profile links pinned libwebp 1.6.0 only for that profile and uses FFmpeg `libwebp_anim` with quality/lossless/compression controls.
- Added a dedicated `libwebp-builder` Docker stage and dependency-aware `export-with-libwebp` path so libwebp never increases unrelated Wasm cores.
- Both animation profiles use WORKERFS input and omit the complete audio stack and FFmpeg CLI.
- Added `BrowserFFmpeg.videoToGifArgs()` / `videoToWebpArgs()`, embedded Builder demos, browser smoke tests, CI matrix jobs, release assets, and matching source/license packaging.
- Manifest schema is now 6 and records exact libwebp pin/link status alongside x264.
- Bumped Builder and runner API version to 1.4.0.

## 1.3.0

- Added the `video-contact-sheet` profile for 12 / 24 / 48 evenly distributed video thumbnails.
- Uses seek-to-keyframe + decode-forward sampling so long recordings do not need to be decoded from beginning to end.
- Added H.264, HEVC/H.265 (including Pixel `hvc1` Main/Main10), VP8, VP9, AV1, MPEG-4, MPEG-1/2, MJPEG, ProRes, and Theora decoding across common containers.
- Added rotation-aware RGB24 thumbnail conversion with libswscale and WORKERFS large-file input.
- The runner assembles one P6 PPM contact sheet and optional JSON sample metadata; browser Canvas handles PNG/JPEG export, avoiding image encoders in Wasm.
- Added `BrowserFFmpeg.videoContactSheetArgs()` and `BrowserFFmpeg.decodePpmOutput()`.
- Added embedded single-HTML demo plus real browser smoke test validating a 12-frame 4x3 sheet.
- Release CI, source checks, licensing docs, and binary/source packaging now include four release profiles.
- Fixed `check-repository.ps1` parsing in GitHub Actions by keeping Markdown backticks out of interpolated PowerShell strings and hardening version-pin regex construction.

## 1.2.0

- Added the `media-inspector` profile: a read-only FFmpeg/libavformat runner that emits structured JSON without decoding or encoding media.
- Reports container/file size/duration/total bitrate, stream codec/tag/profile/level, video resolution/FPS/pixel format/color/HDR/rotation, audio sample rate/channel layout/bitrate, metadata, chapters, and subtitles.
- Added common MP4/MOV, MKV/WebM, AVI, MPEG-TS/PS, FLV, ASF, Ogg, MP3, WAV, FLAC, AAC, AC-3/E-AC-3 and raw H.264/HEVC/AV1/IVF demux support plus lightweight stream parsers.
- Media Inspector uses WORKERFS so browser `File`/`Blob` inputs are not copied wholesale into MEMFS.
- Keep the Media Inspector Wasm LGPL-2.1-or-later: no GPL-only FFmpeg components, x264, decoder, encoder, muxer, filter, swscale, or swresample are linked.
- Added `BrowserFFmpeg.mediaInspectorArgs()` and `BrowserFFmpeg.decodeJsonOutput()` helpers plus an embedded single-HTML demo.
- Added a real browser smoke test that inspects the existing H.264/AAC MP4 fixture and verifies structured report fields.
- Fixed two-channel AAC inspection when FFmpeg exposes only an unspecified channel count: report a friendly `stereo` default while preserving the raw layout and marking the value as inferred.
- Browser compatibility / “Media Doctor” policy stays outside Wasm; apps can combine deterministic report facts with `canPlayType()` / MediaCapabilities.
- GitHub Actions and release packaging now build, smoke-test, and publish all three release profiles.
- Fixed the Windows smoke-test harness to accept profile-specific PASS details such as `streams=2_bytes=...` instead of only the legacy `bytes=...` form.
- Browser stderr diagnostics now use best-effort shared reads so a temporary Chrome file lock cannot mask the real smoke-test result.

## 1.1.0

- Added the `lossless-video-cutter` profile for packet-level cuts without decoding or re-encoding.
- Added keyframe-aligned `--start` / `--end` handling and optional audio removal.
- Added `BrowserFFmpeg.losslessVideoCutterArgs()`.
- Added profile-specific Emscripten WORKERFS support; the cutter can read browser File/Blob inputs on demand without copying the entire input into MEMFS first.
- Keep the cutter core LGPL-2.1-or-later by leaving GPL-only FFmpeg components and x264 disabled; video-compressor remains GPL-2.0-or-later.
- Use a two-seek keyframe selection path so interleaved audio packets at the selected start are not accidentally dropped.
- Generalized profile metadata so each profile declares required FFmpeg config, link libraries, x264 usage, capabilities, and binary license metadata.
- Generalized browser smoke tests to profile-specific test bodies.
- Fixed the cutter smoke assertion to parse the combined `requested-start` / `actual-start` status line and validate the reported keyframe boundary.
- Main/release GitHub Actions now build and smoke-test both release profiles.
- Release packaging now publishes one binary ZIP and BUILDINFO per profile plus one matching corresponding-source archive.

## 1.0.0 - First public release

- Promote the compact/public-libav architecture to the first stable public version.
- Keep the root repository source under MIT while clearly separating generated GPL-covered FFmpeg/x264 Wasm artifacts.
- Add `THIRD_PARTY_NOTICES.md`, release licensing documentation, and exact Emscripten source pinning.
- Add a tag-driven GitHub Release workflow that rebuilds and runs the real browser smoke test before publishing.
- Add automated `BUILDINFO.txt`, SHA-256 checksums, a binary integration bundle, and a corresponding-source archive.
- Add `.gitattributes` so shell/Docker inputs stay LF on Windows clones and the MP4 smoke fixture is always treated as binary.
- Treat `.gitattributes` as recommended repository metadata rather than a hard build prerequisite, so CI reports a warning instead of failing if a copy/push step omits the dotfile.
- Corresponding-source packaging includes exact FFmpeg, x264, Emscripten sources plus the complete Builder recipe used for the release.
- Require the pushed tag to exactly match `BUILDER_VERSION`; release creation uses `gh release create --verify-tag`.

## 0.4.0 - Single public-libav builder with browser smoke test

- Remove the generic pthread/SharedArrayBuffer CLI build path and keep one public-libav runner architecture.
- Flatten generated assets to `dist/<profile>/ffmpeg.js` and `ffmpeg.wasm`.
- Rename runtime/profile/runner files to remove dual-mode naming.
- Add a tiny H.264 + AAC MP4 fixture and package a self-contained `smoke-test.html` with every build.
- Run a real headless Edge/Chrome transcode automatically after local and CI builds.
- Validate the output MP4 (`ftyp`, `moov`, `mdat`, `avc1`, `mp4a`) so FFmpeg upgrades that compile but fail at runtime are rejected.
- Simplify single-HTML packaging and `demo.bat` around the one supported architecture.
- Simplify Docker stages to `x264-builder`, `ffmpeg-source`, `wasm-builder`, and one `export` target.

## 0.3.7 - Direct embedded core script in compact Worker

- Fix packed `file://` single-HTML startup failing when a blob Worker calls `importScripts()` on a second `blob:null/...` URL.
- Compact runtime now builds each Worker from one Blob containing both the generated Emscripten core JS and the compact Worker body, eliminating nested blob-script loading.
- Hosted compact mode now fetches the core JS as text before Worker creation and uses the same execution path as embedded mode.
- Keep the Worker Blob URL alive until the run finishes, then terminate the Worker and revoke the URL together.
- Add a regression check that compact runtime does not use `importScripts()`.

## 0.3.6 - Single-HTML placeholder validation

- Fix `pack-single-html.bat` rejecting the intentional compact-runtime progress sentinel `__FFMPEG_COMPACT_PROGRESS__`.
- Restrict unresolved-placeholder validation to the packaging namespaces (`__COMPACT_*__` and `__CLI_*__`) instead of treating every uppercase double-underscore token as a template placeholder.
- Include the actual unresolved packaging token in the error message and add a repository regression check.

## 0.3.5 - FFmpeg 9 buffer-sink array options

- Fix compact runner startup on FFmpeg 9.0.1.
- Replace removed buffer-sink aliases (`pix_fmts`, `sample_fmts`, `sample_rates`, `ch_layouts`) with the typed array options (`pixel_formats`, `sample_formats`, `samplerates`, `channel_layouts`).
- Use `av_opt_set_array()` for video/audio sink constraints.
- Add precise sink-option error logging to make future FFmpeg API migrations easier to diagnose.

## 0.3.4 - Robust Wasm loading inside blob Workers

- Fixed compact runtime startup failing with `Failed to parse URL from ffmpeg-compact.wasm` when the generated Emscripten glue runs inside the runtime's blob Worker.
- Compact now instantiates the already-transferred Wasm bytes directly with `Module.instantiateWasm`, so hosted and `file://` single-HTML execution do not depend on resolving a relative `.wasm` URL.
- Explicitly preserves `wasmBinary`, `instantiateWasm`, `locateFile`, `print`, and `printErr` through `INCOMING_MODULE_JS_API` so Emscripten upgrades cannot silently remove the runtime hooks.
- Applied the same direct-main-Wasm loading strategy to cli mode while preserving the pthread worker URL mapping.
- Added repository checks for the custom Wasm loader contract.

## 0.3.3 - Correct FFmpeg thread detection and BuildKit targeting

- Fixed the FFmpeg 9.x pthread validation: FFmpeg writes detected thread backends as `HAVE_PTHREADS=yes` / `HAVE_THREADS=yes`, not `CONFIG_PTHREADS=yes`.
- Compact mode now explicitly rejects `HAVE_PTHREADS=yes` and `HAVE_THREADS=yes`, keeping its SharedArrayBuffer-free contract verifiable.
- Local builds now invoke `docker buildx build` without forcing a builder name, so the selected Docker Desktop builder is preserved while BuildKit prunes unrelated sibling stages.
- Renamed the PowerShell Docker argument array to `$DockerArgs` to avoid relying on the automatic `$args` variable.
- Added repository checks covering the correct FFmpeg make variables and the no-forced-builder rule.

## 0.3.2 - Docker context-safe local builds

- Fixed local Docker Desktop builds failing when a Buildx builder named `default` belongs to a different Docker context.
- Local PowerShell and Bash builds now use `docker build`, which follows the active Docker context's default BuildKit builder instead of forcing `--builder default`.
- GitHub Actions keeps using its explicitly configured Buildx builder so the `gha` cache exporter remains available.
- Added a repository regression check that rejects reintroducing a forced local `--builder default`.

## 0.3.1 - Robust Docker Desktop probes

- Fixed Windows PowerShell builds stopping on harmless Docker stderr warnings such as `No blkio throttle.read_bps_device support`.
- Replaced the noisy `docker info` health check with a quiet `docker version` server probe.
- Made Docker and Buildx capability probes ignore stderr text and determine success from the native process exit code.

## 0.3.0 - Dual cli / compact architecture

- Updated the initial dual-mode pin to FFmpeg 9.0.1 and Emscripten 6.0.6.
- Split the builder into `cli` and `compact` modes from the same FFmpeg/x264/Emscripten pins.
- Kept cli on the upstream FFmpeg frontend with Emscripten pthreads and SharedArrayBuffer requirements.
- Added a custom single-thread compact runner using public libav APIs instead of `fftools`.
- Added compact H.264 + AAC MP4 video compression, resize, FPS, CRF/bitrate, audio bitrate and mute controls.
- Added dedicated browser runtimes for cli and compact.
- Added direct-file-oriented compact single-HTML packaging.
- Reworked Docker stages so x264/source layers are shared and cacheable across both modes.
- Added separate `cli.flags` / `compact.flags` per profile and dual-mode GitHub Actions output.
- Made compact runners and single-HTML templates profile-specific for future FFmpeg-powered apps.
- Isolated Docker cache layers so profile/runner edits no longer rebuild x264 or refetch FFmpeg.
- Built x264 as 8-bit 4:2:0 only for the current yuv420p output profiles to reduce unused code.
- Added configure-time capability assertions so removed/renamed FFmpeg components fail before the expensive link step.

## 0.2.0 - Modern FFmpeg pthread frontend

- Fixed the FFmpeg 8.1.2 final-link failure caused by configuring the modern `ffmpeg` frontend without threads.
- Enabled Emscripten pthreads for the FFmpeg CLI while keeping x264's internal codec threading disabled.
- Added early configure checks and pthread worker output.

## 0.1.0

- Initial self-build starter for FFmpeg, x264 and Emscripten.
