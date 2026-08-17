# Validation

Static repository checks verify that:

- pthreads and the upstream FFmpeg program stay disabled
- runtime has no SharedArrayBuffer / cross-origin-isolation dependency
- runtime does not use nested `importScripts()`
- runner uses public libav decode/encode APIs
- FFmpeg 9+ typed buffer-sink array options are used
- single-HTML packaging tokens are complete
- removed dual-mode/CLI files do not reappear
- the smoke-test fixture is a real MP4
- Windows and CI build entry points automatically invoke the browser smoke test
- Builder/Emscripten/FFmpeg/x264 release pins are present
- public release workflow verifies tag = `BUILDER_VERSION` and uses a pre-existing tag
- release preparation includes binary licenses, exact corresponding source, build information, and SHA-256 checksums

The decisive compatibility check is the real browser smoke test. It loads the generated JS/Wasm in a headless Chromium browser, transcodes a tiny H.264/AAC MP4, and verifies the output MP4 contains `ftyp`, `moov`, and `mdat` boxes plus H.264 `avc1` and AAC `mp4a` sample entries.

A public release is stricter still: release preparation fetches and verifies the exact source commits again before packaging.

This environment may not have Docker/Emscripten available, so static validation alone must never be described as a successful full FFmpeg build.
