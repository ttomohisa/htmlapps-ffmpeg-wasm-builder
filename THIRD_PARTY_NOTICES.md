# Third-party notices

This repository contains build/runtime glue written for **FFmpeg WASM Builder** and licensed under the root [MIT License](LICENSE), except where a source file carries an additional preserved notice.

The repository does not vendor FFmpeg, x264, libwebp, or Emscripten source trees. Builds fetch exact upstream revisions recorded in `versions.env`. Tagged GitHub Releases include the matching upstream source revisions and the Builder recipe.

## FFmpeg

`video-compressor` passes `--enable-gpl` and links x264, so that generated core is distributed under GPL-2.0-or-later. `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, and `video-to-webp` enable no GPL-only FFmpeg component and do not link x264, so their generated cores remain under FFmpeg's LGPL-2.1-or-later terms. The exact FFmpeg source revision and applicable license text are included with public releases.

`runners/video-compressor.c` preserves the MIT notice for FFmpeg's `doc/examples/transcode.c`-derived structure. `runners/lossless-video-cutter.c` uses the public remuxing pattern and public libav APIs; it does not decode or encode media. `runners/media-inspector.c` uses public libavformat/libavcodec/libavutil inspection APIs and emits a structured JSON report without decoding frames. `runners/video-contact-sheet.c` uses public libavformat/libavcodec/libswscale APIs to seek/decode selected frames and write an RGB PPM without linking an image encoder. `runners/video-to-animation-common.inc` uses public libavformat/libavcodec/libavfilter APIs for trim/fps/rotation/scale; the GIF profile performs `palettegen` then `paletteuse`, while the WebP profile uses FFmpeg's `libwebp_anim` wrapper. These browser input paths use Emscripten WORKERFS so File/Blob data can be read from a Worker without first copying the entire input into MEMFS.

## x264

x264 is pinned in `versions.env` because the `video-compressor` profile links it. The `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, and `video-to-webp` profiles set `PROFILE_USE_X264=0`; x264 is not linked into those generated Wasm cores.

The open-source x264 build used by the video-compressor profile is GPL-2.0-or-later.

## libwebp

libwebp is pinned in `versions.env` and linked only by `video-to-webp` (`PROFILE_USE_LIBWEBP=1`). The GIF and existing profiles keep `PROFILE_USE_LIBWEBP=0`, so libwebp does not contribute to their Wasm size. The release bundle for `video-to-webp` includes libwebp's `COPYING` and, when present, `PATENTS` notice files.

## Emscripten

Emscripten is available under the MIT and University of Illinois/NCSA Open Source licenses. Generated JavaScript/Wasm may contain Emscripten runtime, musl libc, and compiler-rt code. Release bundles carry the corresponding license/notices from the pinned Emscripten source tree.

## Generated artifacts

The root MIT license applies to the Builder's original source only and does **not** relicense generated `ffmpeg.wasm`. Every binary bundle includes profile-specific `BUILDINFO.txt`, upstream notices, and a pointer to the same tagged release's corresponding-source archive.

This file is engineering documentation, not legal advice.

## libvpx (VP9)

`video-compressor` links libvpx only to provide VP9/WebM output (`PROFILE_USE_LIBVPX=1`). The pinned libvpx source is distributed under a BSD-style license; upstream also provides a separate patent grant in `PATENTS`. Other profiles keep libvpx disabled so it does not add to their Wasm payload.

## Opus

`video-compressor` links libopus only for Opus audio when WebM/VP9 is selected (`PROFILE_USE_LIBOPUS=1`). Opus is distributed under a three-clause BSD license with the upstream royalty-free patent grant described in its `COPYING` file. Other profiles keep libopus disabled.
