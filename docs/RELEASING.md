# Releasing

`main` builds and smoke-tests every profile listed in `.github/workflows/build.yml`. Do not tag a release until all matrix jobs are green.

For v1.9.8:

```text
git tag -a v1.9.8 -m "FFmpeg WASM Builder v1.9.8"
git push origin v1.9.8
```

The tag workflow verifies that the tag matches `BUILDER_VERSION`, rebuilds all current release profiles, runs their real browser smoke tests (including ST and COOP/COEP-hosted MT for `ffmpeg-filter-builder`), then publishes:

```text
ffmpeg-wasm-video-compressor-v1.9.8.zip
ffmpeg-wasm-video-speed-changer-v1.9.8.zip
ffmpeg-wasm-lossless-video-cutter-v1.9.8.zip
ffmpeg-wasm-media-inspector-v1.9.8.zip
ffmpeg-wasm-video-contact-sheet-v1.9.8.zip
ffmpeg-wasm-video-to-gif-v1.9.8.zip
ffmpeg-wasm-video-to-webp-v1.9.8.zip
ffmpeg-wasm-ffmpeg-filter-builder-single-thread-v1.9.8.zip
ffmpeg-wasm-ffmpeg-filter-builder-multi-thread-v1.9.8.zip
ffmpeg-wasm-sources-v1.9.8.tar.gz
BUILDINFO-video-compressor.txt
BUILDINFO-video-speed-changer.txt
BUILDINFO-lossless-video-cutter.txt
BUILDINFO-media-inspector.txt
BUILDINFO-video-contact-sheet.txt
BUILDINFO-video-to-gif.txt
BUILDINFO-video-to-webp.txt
BUILDINFO-ffmpeg-filter-builder-single-thread.txt
BUILDINFO-ffmpeg-filter-builder-multi-thread.txt
SHA256SUMS.txt
```

Each binary ZIP contains its generated core, runtime, manifest, profile-specific `BUILDINFO.txt`, and applicable license notices. The corresponding-source archive contains exact FFmpeg/x264/libvpx/Opus/libwebp/Emscripten source revisions plus the Builder recipe.

## Consuming release assets

Applications should not build against `latest` or an unverified downloaded binary.

Recommended consumer flow:

1. pin the Builder release tag,
2. pin the exact profile asset name,
3. record and verify the release asset SHA-256 (`digest` / `SHA256SUMS.txt`),
4. validate the profile manifest and required runtime contract,
5. embed the verified files at application build time,
6. keep runtime delivery offline when the application requires fully local processing.

A local Builder checkout remains useful for development, but production/release builds should be reproducible from a tagged release asset and its checksum.
