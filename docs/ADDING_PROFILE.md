# Adding another FFmpeg-powered app

A profile consists of FFmpeg component flags, build/link metadata, one public-libav runner, and one real browser smoke test.

For `media-inspector`:

```text
profiles/media-inspector/
├─ profile.env
├─ ffmpeg.flags
└─ README.md

runners/media-inspector.c
tests/smoke-tests/media-inspector.js
```

## 1. Start with the smallest component set

Use `--disable-everything`. Enable only the demuxers, muxers, decoders, encoders, parsers, protocols, filters, and bitstream filters the operation needs.

For remux/inspection tools, do not enable a decoder or encoder merely because the input codec has that name. `avformat` can often inspect/copy compressed packets using codec parameters without decoding them.

## 2. Add `profile.env`

Required fields:

```bash
PROFILE_DISPLAY_NAME="Media Inspector"
PROFILE_USE_X264=0
PROFILE_USE_LIBWEBP=0
PROFILE_USE_WORKERFS=0
PROFILE_BINARY_LICENSE="GPL-2.0-or-later"
PROFILE_OUTPUT_DESCRIPTION="metadata JSON"
PROFILE_REQUIRED_CONFIG=(CONFIG_MOV_DEMUXER CONFIG_MATROSKA_DEMUXER CONFIG_FILE_PROTOCOL)
PROFILE_LINK_LIBS=(libavformat/libavformat.a libavcodec/libavcodec.a libavutil/libavutil.a)
PROFILE_CAPABILITIES_JSON='{"operation":"inspect","arbitraryFfmpegArgs":false}'
```

Keep `PROFILE_LINK_LIBS` minimal. If `PROFILE_USE_X264=0`, the final linker must not include `libx264.a`. Likewise, keep `PROFILE_USE_LIBWEBP=0` unless the profile needs FFmpeg's libwebp wrappers; libwebp then uses its own Docker/export target. Set `PROFILE_USE_WORKERFS=1` only when the browser profile should mount large File/Blob inputs read-only through Emscripten WORKERFS.

## 3. Write a public-libav runner

Use installed FFmpeg public headers/APIs only. Do not copy or depend on `fftools` internals. Keep pthreads disabled unless the entire runtime architecture is intentionally redesigned.

## 4. Add a real smoke test

`tests/smoke-tests/<profile>.js` is inserted into the generic browser smoke-test page. It must execute the real profile operation and validate meaningful output. Compile-only success is not sufficient.

## 5. Build

```text
build.bat media-inspector
```

The build is successful only after the browser smoke test prints:

```text
[OK] Smoke test passed.
```

## Current examples

`video-compressor` demonstrates a decode/filter/encode profile with x264. `lossless-video-cutter` demonstrates a much smaller packet-copy/remux profile with no decoder, encoder, filter, or x264 in the final Wasm.
