# FFmpeg Filter Builder profile

Dedicated FFmpeg WASM runtime for Browser Kitty FFmpeg Filter Builder.

## Threading

This is the first dual-runtime profile. A normal build produces both:

- `single-thread/`: no SharedArrayBuffer, suitable for offline/file:// single-HTML packaging.
- `multi-thread/`: pthread build, requires SharedArrayBuffer and cross-origin isolation when hosted.

Both variants are generated from the same FFmpeg/x264 pins, profile flags and public-libav runner. The initial MT contract prewarms 8 pthread workers while bounding codec decode/encode work to 4 threads; filtergraph slice-threading is kept at 1 until the real Filter Builder workloads are benchmarked.

## Current scope

The profile supports a caller-supplied video/audio filter chain, H.264/AAC MP4 output, common input decoders, and the built-in filter catalog that does not require additional font libraries. v1.9.5 adds video `trim` and bounded time-range rendering through `--start-time` / `--duration`. The runner normalizes timestamps with `setpts` / `asetpts` and keeps audio aligned with `atrim`; these companion filters were already present in v1.9.4.

`drawtext` is deliberately not enabled yet. It will be added together with the embedded-font/freetype dependency work instead of exposing a filter that cannot render the Browser Kitty v1.0 text node correctly.

The public-libav runner is not the upstream `ffmpeg` CLI and does not accept arbitrary shell arguments.

## PNG / zlib

PNG input uses FFmpeg's native PNG decoder. FFmpeg 9.0.1 selects `inflate_wrapper` for that decoder, so this profile explicitly sets `PROFILE_USE_ZLIB=1`, passes `--enable-zlib`, and links the zlib system port provided by the pinned Emscripten toolchain. This applies to both ST and MT builds; unrelated profiles do not inherit zlib.

## Time-range rendering

`BrowserFFmpeg.ffmpegFilterBuilderArgs()` accepts `startTimeSeconds` and `durationSeconds`. The runner prepends the range filters before the caller-supplied graph and resets output timestamps, and stops demux/decode shortly after the requested end (with a one-second guard for reordering/keyframe dependencies). The filter graph remains the authority for exact output bounds.
