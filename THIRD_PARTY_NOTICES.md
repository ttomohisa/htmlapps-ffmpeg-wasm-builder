# Third-party notices

This repository contains build/runtime glue written for **FFmpeg WASM Builder** and licensed under the root [MIT License](LICENSE), except where a source file carries an additional preserved notice.

The repository does not vendor FFmpeg, x264, or Emscripten source trees. Builds fetch exact upstream revisions recorded in `versions.env`. Tagged GitHub Releases include the matching upstream source revisions and the Builder recipe.

## FFmpeg

`video-compressor` passes `--enable-gpl` and links x264, so that generated core is distributed under GPL-2.0-or-later. `lossless-video-cutter` enables no GPL-only FFmpeg component and does not link x264, so its generated core remains under FFmpeg's LGPL-2.1-or-later terms. The exact FFmpeg source revision and applicable license text are included with public releases.

`runners/video-compressor.c` preserves the MIT notice for FFmpeg's `doc/examples/transcode.c`-derived structure. `runners/lossless-video-cutter.c` uses the public remuxing pattern and public libav APIs; it does not decode or encode media. Its browser input path uses Emscripten WORKERFS so File/Blob data can be read from a Worker without first copying the entire input into MEMFS.

## x264

x264 is pinned in `versions.env` because the `video-compressor` profile links it. The `lossless-video-cutter` profile sets `PROFILE_USE_X264=0`; x264 is not linked into that profile's generated Wasm.

The open-source x264 build used by the video-compressor profile is GPL-2.0-or-later.

## Emscripten

Emscripten is available under the MIT and University of Illinois/NCSA Open Source licenses. Generated JavaScript/Wasm may contain Emscripten runtime, musl libc, and compiler-rt code. Release bundles carry the corresponding license/notices from the pinned Emscripten source tree.

## Generated artifacts

The root MIT license applies to the Builder's original source only and does **not** relicense generated `ffmpeg.wasm`. Every binary bundle includes profile-specific `BUILDINFO.txt`, upstream notices, and a pointer to the same tagged release's corresponding-source archive.

This file is engineering documentation, not legal advice.
