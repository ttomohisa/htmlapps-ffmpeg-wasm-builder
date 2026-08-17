# License files in public releases

This source repository is MIT-licensed at the root. Third-party source trees are intentionally **not vendored** here.

For each tagged public release, `scripts/prepare-release.sh` fetches the exact revisions pinned in `versions.env` and copies their unmodified upstream license files into the binary bundle:

- `FFmpeg-LICENSE.md` and `FFmpeg-COPYING.GPLv2` from the pinned FFmpeg tree
- `x264-COPYING` from the pinned x264 tree
- `Emscripten-LICENSE`, `Emscripten-musl-COPYRIGHT`, and `Emscripten-compiler-rt-LICENSE.txt` from the pinned Emscripten tree
- `Builder-MIT.txt` from this repository

The same release also contains `ffmpeg-wasm-sources-<version>.tar.gz`, which includes the exact FFmpeg, x264, and Emscripten source revisions plus this builder recipe.

See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) and [docs/LICENSES.md](../docs/LICENSES.md).
