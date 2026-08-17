# Architecture

## Goal

Track current FFmpeg without carrying a fork of `fftools`, while keeping the browser artifact small and usable without SharedArrayBuffer.

```text
FFmpeg source + x264 + Emscripten
              ↓
       static libav libraries
              ↓
      runners/<profile>.c
              ↓
        ffmpeg.js / .wasm
              ↓
        Web Worker runtime
```

The upstream `ffmpeg` command-line frontend is intentionally not linked. The profile runner uses public FFmpeg APIs and exposes only the operations needed by the app.

## Threading model

FFmpeg is configured with all thread backends disabled, and x264 is built with `--disable-thread`. The browser runtime therefore does not require Emscripten pthreads, SharedArrayBuffer, or cross-origin isolation.

Encoding still runs in a normal browser Web Worker so the page UI stays responsive.

## Embedded/file:// runtime

The generated Emscripten JS and the runtime Worker body are concatenated into one Blob. Wasm bytes are transferred into the Worker and supplied through `instantiateWasm`, avoiding nested `importScripts()` and relative `.wasm` URL resolution. This is what allows the packaged single HTML to run from `file://`.

## Docker cache layers

`docker/Dockerfile` keeps expensive work separated:

1. `toolchain-base`: Emscripten + build packages
2. `x264-builder`: exact x264 commit, single-threaded
3. `ffmpeg-source`: exact FFmpeg commit checkout
4. `wasm-builder`: profile flags + runner + final FFmpeg library build/link
5. `package-builder`: runtime + fixture packaging only; does not rebuild FFmpeg
6. `export`: clean artifact output

Changing the runner/profile should not redownload FFmpeg or rebuild x264. Changing only the FFmpeg pin should keep the x264 layer cached.

## Upgrade boundaries

- Removed/renamed codec, demuxer, muxer, parser, protocol, or filter: configure fails.
- Public libav API change: runner C compile/link fails.
- Emscripten setting/runtime contract change: Wasm link or browser startup fails.
- Runtime-only behavior regression: browser smoke test fails even if the build itself succeeded.

This last layer is important: the smoke test exists specifically to catch upgrades that compile cleanly but no longer transcode.

## FFmpeg 9 buffer-sink compatibility

The runner constrains buffer sinks using typed array AVOptions (`pixel_formats`, `sample_formats`, `samplerates`, `channel_layouts`) through `av_opt_set_array()`. Removed legacy aliases are intentionally not used.
