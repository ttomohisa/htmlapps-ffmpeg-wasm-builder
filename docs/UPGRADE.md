# Upgrading FFmpeg / Emscripten / x264

Change one dependency at a time.

1. Run `check-updates.bat`.
2. Copy only the FFmpeg candidate into `versions.env` first.
3. Run `build.bat`.
4. Require the browser smoke test to pass.
5. Commit that update before trying Emscripten or x264.

A successful Docker compile is not enough. `build.bat` automatically launches a headless Chromium browser and transcodes `tests/fixtures/smoke-input.mp4`. The upgrade should be considered usable only when that test reports `SMOKE_TEST_PASS`.

When an upgrade fails, use the stage to narrow the cause:

- FFmpeg configure: component name/dependency changed.
- FFmpeg library compile: upstream source/toolchain incompatibility.
- runner compile/link: public API changed.
- browser smoke test: generated Wasm loads but actual decode/filter/encode/mux behavior regressed.

The fixture intentionally contains H.264 video and AAC audio, so the smoke test exercises the main video-compressor path rather than only calling `--version`.
