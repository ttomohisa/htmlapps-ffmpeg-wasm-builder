# Licensing and public distribution

## Repository source

The original build scripts, browser runtime, documentation, and other glue in this repository are released under the root MIT license. `runners/video-compressor.c` also preserves the MIT notice from FFmpeg's `doc/examples/transcode.c` for the portions derived from that example.

## Generated FFmpeg Wasm

The default profile enables libx264 and passes `--enable-gpl` to FFmpeg. FFmpeg's own licensing documentation states that enabling GPL components changes the resulting FFmpeg build to GPL-2.0-or-later, and lists libx264 as a GPL-compatible external library that requires `--enable-gpl`. Therefore **do not label generated `ffmpeg.wasm` as MIT**.

Emscripten-generated JavaScript/Wasm may contain Emscripten runtime, musl libc, and compiler-rt code. The release bundle therefore includes the pinned Emscripten license plus the musl and compiler-rt license/notices taken from that exact Emscripten source revision.

## What a tagged release contains

The release workflow builds and smoke-tests the profile first. Only then does it create release assets:

```text
ffmpeg-wasm-video-compressor-v1.0.0.zip
ffmpeg-wasm-sources-v1.0.0.tar.gz
BUILDINFO.txt
SHA256SUMS.txt
```

The binary ZIP contains the generated JS/Wasm, browser runtime, manifest, build information, notices, and upstream license files. The source archive contains the exact FFmpeg/x264/Emscripten source revisions and a copy of the build recipe used for that release.

The release process deliberately keeps the binary and its corresponding source available from the **same GitHub Release**.

See `THIRD_PARTY_NOTICES.md` and `docs/RELEASING.md`. This document is engineering guidance, not legal advice.
