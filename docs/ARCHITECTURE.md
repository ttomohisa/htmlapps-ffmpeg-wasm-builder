# Architecture

The Builder does not compile the upstream FFmpeg CLI. It links small profile-specific C runners against public FFmpeg libraries.

```text
FFmpeg source + profile configure flags
               ↓
      static public libav libraries
               ↓
          profile runner.c
               ↓
      Emscripten modularized Wasm
               ↓
   Blob Worker + transferred Wasm bytes
```

All current profiles are single-threaded. No pthread flags are linked, so SharedArrayBuffer and cross-origin isolation are unnecessary.

## Profile-aware linking

`profiles/<profile>/profile.env` defines `PROFILE_LINK_LIBS`, `PROFILE_USE_X264`, `PROFILE_USE_LIBWEBP`, and `PROFILE_USE_WORKERFS`. The linker only includes those FFmpeg/external/runtime pieces (x264 and libwebp are separate optional stages). This is what lets packet-copy tools stay much smaller than a transcoder even though they share the same Builder.

`video-compressor` links avfilter/avformat/avcodec/swresample/swscale/avutil plus x264. `lossless-video-cutter` links only avformat/avcodec/avutil and performs no decode/encode/filter work. The cutter additionally links Emscripten WORKERFS (`-lworkerfs.js`) so browser File/Blob input can be read on demand inside the Worker without a whole-file MEMFS copy.
