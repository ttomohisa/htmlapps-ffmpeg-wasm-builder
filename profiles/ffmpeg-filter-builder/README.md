# FFmpeg Filter Builder profile

Dedicated FFmpeg WASM runtime for Browser Kitty FFmpeg Filter Builder.

## Threading

This is the first dual-runtime profile. A normal build produces both:

- `single-thread/`: no SharedArrayBuffer, suitable for offline/file:// single-HTML packaging.
- `multi-thread/`: pthread build, requires SharedArrayBuffer and cross-origin isolation when hosted.

Both variants are generated from the same FFmpeg/x264 pins, profile flags and public-libav runner. The initial MT contract prewarms 8 pthread workers while bounding codec decode/encode work to 4 threads; filtergraph slice-threading is kept at 1 until the real Filter Builder workloads are benchmarked.

## Current scope

The profile supports a caller-supplied video/audio filter chain, H.264/AAC MP4 output, common input decoders, and the Browser Kitty filter catalog including `drawtext`. v1.9.5 adds video `trim` and bounded time-range rendering through `--start-time` / `--duration`. The runner normalizes timestamps with `setpts` / `asetpts` and keeps audio aligned with `atrim`; these companion filters were already present in v1.9.4.

`drawtext` is enabled in v1.9.8 with the pinned Emscripten FreeType and HarfBuzz ports. The runtime does not bundle a font by itself; the app passes an embedded/local font file through the existing virtual filesystem input contract and refers to that absolute path from `drawtext=fontfile=...`.

The public-libav runner is not the upstream `ffmpeg` CLI and does not accept arbitrary shell arguments.

## v1.9.8 drawtext / font libraries

The Filter Builder profile enables `libfreetype`, `libharfbuzz`, and the `drawtext` filter. Both ST and MT builds link Emscripten's pinned ports via `-sUSE_FREETYPE=1` and `-sUSE_HARFBUZZ=1`. Font bytes remain an app asset, not a Builder runtime asset, so different Browser Kitty apps can choose their own licensed local font without adding runtime network access.

## PNG / zlib

PNG input uses FFmpeg's native PNG decoder. FFmpeg 9.0.1 selects `inflate_wrapper` for that decoder, so this profile explicitly sets `PROFILE_USE_ZLIB=1`, passes `--enable-zlib`, and links the zlib system port provided by the pinned Emscripten toolchain. This applies to both ST and MT builds; unrelated profiles do not inherit zlib.

## Time-range rendering

`BrowserFFmpeg.ffmpegFilterBuilderArgs()` accepts `startTimeSeconds` and `durationSeconds`. The runner prepends the range filters before the caller-supplied graph and resets output timestamps, and stops demux/decode shortly after the requested end (with a one-second guard for reordering/keyframe dependencies). The filter graph remains the authority for exact output bounds.


## v1.9.7 speed timestamp fix

Runner 0.2.2 uses a minimum 90 kHz video filter time base and rescales decoded PTS before frames enter the graph. This preserves sub-frame timestamp precision when `setpts` compresses time (for example `PTS/1.5`) and keeps encoded/muxed DTS strictly monotonic. The H.264 encoder uses the same fine-grained clock instead of the old `1/fps` time base.

## v1.9.6 fixes

v1.9.6 hardens bounded preview rendering for long inputs: filter-source EOF after `trim` / `atrim` is a normal completion condition, not a runner failure. The runner also probes video filter output geometry before encoder creation, so graph filters that change frame dimensions are preserved in the encoded MP4.
