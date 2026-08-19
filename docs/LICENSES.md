# Licensing and release packaging

The root `LICENSE` applies to the Builder's original source. Generated WebAssembly contains third-party code and is not relicensed by that MIT file.

`video-compressor` configures FFmpeg with `--enable-gpl` and links x264, so its generated core is GPL-2.0-or-later. `lossless-video-cutter`, `media-inspector`, and `video-contact-sheet` enable no GPL-only component and do not link x264, so their generated cores are LGPL-2.1-or-later. Each binary bundle carries the license text applicable to that profile.

Every tagged release keeps binary artifacts, profile-specific build information, upstream notices, and the exact corresponding-source archive in the same GitHub Release. See `THIRD_PARTY_NOTICES.md` for the component summary.

This documentation is an engineering packaging policy, not legal advice.
