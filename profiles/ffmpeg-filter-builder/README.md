# FFmpeg Filter Builder profile

Dedicated FFmpeg WASM runtime for Browser Kitty FFmpeg Filter Builder.

## Threading

This is the first dual-runtime profile. A normal build produces both:

- `single-thread/`: no SharedArrayBuffer, suitable for offline/file:// single-HTML packaging.
- `multi-thread/`: pthread build, requires SharedArrayBuffer and cross-origin isolation when hosted.

Both variants are generated from the same FFmpeg/x264 pins, profile flags and public-libav runner. The initial MT contract prewarms 8 pthread workers while bounding codec decode/encode work to 4 threads; filtergraph slice-threading is kept at 1 until the real Filter Builder workloads are benchmarked.

## Initial v0.1 scope

The first profile intentionally proves the runtime architecture before the complete graph runner is implemented. It supports a caller-supplied video/audio filter chain, H.264/AAC MP4 output, common input decoders, and the planned built-in filter catalog that does not require additional font libraries.

`drawtext` is deliberately not enabled yet. It will be added together with the embedded-font/freetype dependency work instead of exposing a filter that cannot render the Browser Kitty v1.0 text node correctly.

The public-libav runner is not the upstream `ffmpeg` CLI and does not accept arbitrary shell arguments.

## PNG / zlib

PNG input uses FFmpeg's native PNG decoder. FFmpeg 9.0.1 selects `inflate_wrapper` for that decoder, so this profile explicitly sets `PROFILE_USE_ZLIB=1`, passes `--enable-zlib`, and links the zlib system port provided by the pinned Emscripten toolchain. This applies to both ST and MT builds; unrelated profiles do not inherit zlib.
