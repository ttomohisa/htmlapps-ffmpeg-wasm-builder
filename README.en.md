# FFmpeg WASM Builder

Self-build a focused browser FFmpeg WebAssembly core from pinned FFmpeg + x264 + Emscripten sources. No prebuilt `@ffmpeg/ffmpeg` / `@ffmpeg/core` binary is consumed.

The project intentionally uses one architecture: a profile-specific runner linked against FFmpeg's public `libav*` APIs. The upstream `ffmpeg` CLI is not linked, pthreads are disabled, and the browser runtime requires neither SharedArrayBuffer nor cross-origin isolation.

`build.bat` / `./build.sh` produces `ffmpeg.js`, `ffmpeg.wasm`, compressed copies, a manifest, and a self-contained smoke-test page. A real headless Chromium browser then transcodes a tiny H.264/AAC MP4 fixture and validates the resulting MP4 structure and codecs.

## v1.0.0 release pins

All build/source pins live in `versions.env`: FFmpeg 9.0.1, Emscripten 6.0.6, and an exact x264 commit. The Emscripten source commit is pinned separately from the Docker image tag so public releases can ship matching source.

## Public releases

Push `main` first and require the normal build workflow to pass. Then push a tag that exactly matches `BUILDER_VERSION`, for example:

```text
git tag -a v1.0.0 -m "FFmpeg WASM Builder v1.0.0"
git push origin v1.0.0
```

The tag workflow rebuilds, runs the browser smoke test, fetches the exact FFmpeg/x264/Emscripten source revisions, and publishes a binary bundle, corresponding-source archive, build information, and SHA-256 checksums. See `docs/RELEASING.md`.

## Using it in apps

Prefer downloading a **fixed Builder release while updating/building the consuming app**, then ship the assets with that app. Do not make a production browser session depend on GitHub Releases at runtime. See `docs/USING_IN_APPS.md`.

## Licensing

The root MIT license covers this repository's original builder/runtime source. It does **not** relicense generated `ffmpeg.wasm`. The default video-compressor profile enables `--enable-gpl` and links x264, so generated FFmpeg/x264 Wasm artifacts are distributed under the applicable GPL-2.0-or-later terms. Tagged releases attach exact corresponding source and upstream license files. See `THIRD_PARTY_NOTICES.md` and `docs/LICENSES.md`.
