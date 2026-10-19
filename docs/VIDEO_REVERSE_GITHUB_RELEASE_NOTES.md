# Video Reverse FFmpeg WASM (Builder 1.10.6)

This is the **profile-specific binary and corresponding-source distribution** for the `video-reverse` FFmpeg WASM runtime used by Reverse Clip.

- **Single-thread:** `ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip` (standalone browser runtime without cross-origin isolation).
- **Multi-thread:** `ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip` (Emscripten pthreads, requires SharedArrayBuffer / cross-origin isolation).
- **Corresponding source:** `ffmpeg-wasm-sources-v1.10.6.tar.gz`, including Builder build scripts, matching FFmpeg/x264/Emscripten upstream source trees and recorded dependency revisions.
- **Metadata:** two original `BUILDINFO-video-reverse-*.txt` files and `SHA256SUMS.txt`, plus `VIDEO_REVERSE_LICENSE_ADDENDUM.md`.

## Original compiled source and tag naming

The original unchanged runtime binaries were built from Builder commit
[`8e60da7cf9de644ce3f6cce9c007b17e6a385c53`](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/commit/8e60da7cf9de644ce3f6cce9c007b17e6a385c53) and report **Builder 1.10.6**.

The embedded `BUILDINFO.txt` describes `Release tag: v1.10.6`, the general Builder version label at the time of compilation. This **separate profile-specific Release** uses `video-reverse-v1.10.6`, because the already-published Builder-wide `v1.10.6` Release does not contain these `video-reverse` binaries. Existing audited binary and source bytes are preserved unchanged; the Git tag for this profile points to the exact compiled source commit, not the later documentation/packaging branch.

## License and corresponding source

The generated `video-reverse` FFmpeg/x264 ST and MT cores are **GPL-2.0-or-later**. These compiled binaries include original FFmpeg, x264 and Emscripten license texts. The original ZIPs' generic `THIRD_PARTY_NOTICES.md` predates the dedicated profile name; the separately provided `VIDEO_REVERSE_LICENSE_ADDENDUM.md` clarifies the profile's GPL/x264 classification and tag naming. The addendum was written before publication and retains its historical review wording; this GitHub Release is the versioned public download location for the files.

The Builder repository's MIT license for its own source does not relicense the GPL-licensed FFmpeg/x264 WebAssembly cores. Reverse Clip's app source is separately MIT-licensed.

## Original SHA-256 (not repacked)

| File | SHA-256 |
| --- | --- |
| `ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip` | `9087bf4d03b29d141e78c93e9de62a237f2d408f24b43caeef26072e6d3b21b0` |
| `ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip` | `b577be491754ecfa96d0b86021184b82b2fc280f8881bd73cb5d52d83914d3bd` |
| `ffmpeg-wasm-sources-v1.10.6.tar.gz` | `9ceb47a5f46fb3484f8b1f6a8fa37e6d0dc4144aa09d78939ac9d23e8ab5ce92` |
| `VIDEO_REVERSE_LICENSE_ADDENDUM.md` | `e011d66e3451dbf5ffa5f67f26636749664a69d285a3cfc665a71e36635e15f3` |

`SHA256SUMS.txt` records checksums for the two binary ZIPs, source TAR and both BUILDINFO files. The additional GPL disclosure hash is given above.

## Status and compatibility

This is the **WASM profile Release**, not a Reverse Clip app release. The engine has passed real-browser ST/MT conversion checks including reverse, ping-pong and supported audio operations. Workloads and device/browser capabilities vary. MT needs appropriate isolation headers or an equivalent tested isolation mechanism. Original user media is handled locally in the consuming app; this release contains no user media.

For the consuming app, obtain these exact versioned assets and their hashes, then build and validate its ST/MT HTML distribution before updating Browser Kitty.
