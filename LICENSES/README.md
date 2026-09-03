# License files in public releases

The root MIT license covers this Builder's original source. Third-party source trees are not vendored in the repository.

Tagged releases fetch the exact revisions pinned in `versions.env` and package applicable upstream notices beside each profile binary:

- FFmpeg license text plus `FFmpeg-COPYING.GPLv2` for GPL profiles or `FFmpeg-COPYING.LGPLv2.1` for LGPL profiles
- x264 `COPYING` only for profiles that actually link x264
- libwebp `COPYING` / `PATENTS` only for profiles that actually link libwebp
- libvpx `LICENSE` / `PATENTS` only for profiles that actually link libvpx
- Opus `COPYING` only for profiles that actually link libopus
- Emscripten license, musl notice, and compiler-rt license
- `Builder-MIT.txt`

The same GitHub Release also contains `ffmpeg-wasm-sources-<version>.tar.gz` with the exact FFmpeg, x264, libwebp, libvpx, Opus, and Emscripten source revisions plus this Builder recipe.

See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) and [docs/LICENSES.md](../docs/LICENSES.md).
