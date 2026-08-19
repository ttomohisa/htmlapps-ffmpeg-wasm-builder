# Validation

Static repository checks verify that:

- pthreads and the upstream FFmpeg program stay disabled
- runtime has no SharedArrayBuffer / cross-origin-isolation dependency
- runtime does not use nested `importScripts()`
- runners use only their intended public libav APIs (transcode, stream-copy, or inspect-only)
- FFmpeg 9+ typed buffer-sink array options are used
- single-HTML packaging tokens are complete
- removed dual-mode/CLI files do not reappear
- the smoke-test fixture is a real MP4
- Media Inspector stays decoder/encoder/muxer/filter-free and emits structured JSON through WORKERFS input
- Windows and CI build entry points automatically invoke the browser smoke test
- Builder/Emscripten/FFmpeg/x264 release pins are present
- public release workflow verifies tag = `BUILDER_VERSION` and uses a pre-existing tag
- release preparation includes binary licenses, exact corresponding source, build information, and SHA-256 checksums

The decisive compatibility check is the real browser smoke test. Each profile executes its actual operation in headless Chromium. `video-compressor` transcodes the tiny H.264/AAC MP4 and validates the output container/codecs; `lossless-video-cutter` performs a real stream-copy cut; `media-inspector` reads the same fixture through WORKERFS and validates the structured JSON report (container, H.264 video, AAC stereo audio, resolution, frame rate, duration, and bitrate); `video-contact-sheet` seeks/decodes 12 frames and validates the P6 RGB dimensions, image variation, grid metadata, codec, and sample timestamps.

A public release is stricter still: release preparation fetches and verifies the exact source commits again before packaging.

This environment may not have Docker/Emscripten available, so static validation alone must never be described as a successful full FFmpeg build.
