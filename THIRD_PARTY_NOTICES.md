# Third-party notices

This repository contains build/runtime glue written for **FFmpeg WASM Builder** and licensed under the root [MIT License](LICENSE), except where a source file carries an additional preserved notice.

The repository does **not** vendor FFmpeg, x264, or Emscripten source trees. Builds fetch exact upstream revisions recorded in `versions.env`. Public GitHub Releases created by this repository include a corresponding-source archive containing those exact source revisions and the build recipe used for the release.

## FFmpeg

- Project: FFmpeg
- Source: `FFMPEG_REPOSITORY`, `FFMPEG_REF`, and `FFMPEG_COMMIT` in `versions.env`
- Default upstream license: LGPL-2.1-or-later, with optional GPL components
- This builder passes `--enable-gpl` because the default profile links libx264. FFmpeg documents that enabling GPL components changes the resulting FFmpeg build to GPL-2.0-or-later.
- `runners/video-compressor.c` derives portions of its decode/filter/encode structure from FFmpeg's MIT-licensed `doc/examples/transcode.c`; that MIT notice is preserved at the top of the runner source.

## x264

- Project: x264
- Source: `X264_REPOSITORY` / `X264_FALLBACK_REPOSITORY` and `X264_COMMIT` in `versions.env`
- The default open-source x264 build used here is GPL-2.0-or-later.

## Emscripten

- Project: Emscripten
- Source: `EMSCRIPTEN_REPOSITORY`, `EMSCRIPTEN_REF`, and `EMSCRIPTEN_COMMIT` in `versions.env`
- Emscripten is available under the MIT and University of Illinois/NCSA Open Source licenses. Generated JavaScript/Wasm may contain Emscripten runtime, musl libc, and compiler-rt code. Release bundles therefore carry the exact Emscripten license plus the bundled musl and compiler-rt license/notices from the pinned Emscripten source tree.

## Generated artifacts

The root MIT license applies to this repository's original builder/runtime source. It does **not** relicense generated `ffmpeg.wasm`. The default `video-compressor` profile combines FFmpeg configured with `--enable-gpl` and x264, so generated FFmpeg/x264 WebAssembly artifacts must be distributed under the applicable GPL-2.0-or-later terms.

The release workflow places the exact upstream license files next to binary artifacts and provides the exact corresponding source archive in the same GitHub Release.

This file is engineering documentation, not legal advice.
