# Releasing

`main` builds and smoke-tests every profile listed in `.github/workflows/build.yml`. Do not tag a release until all matrix jobs are green.

For v1.10.0:

```text
git switch main
git pull --ff-only
git tag -a v1.10.0 -m "FFmpeg WASM Builder v1.10.0"
git push origin v1.10.0
```

The tag workflow verifies that the tag matches `BUILDER_VERSION`, rebuilds all current release profiles, and runs their real-browser smoke tests before publishing. `ffmpeg-filter-builder` still ships both single-thread and COOP/COEP-hosted multi-thread variants.

v1.10.0 publishes:

```text
ffmpeg-wasm-video-compressor-v1.10.0.zip
ffmpeg-wasm-video-speed-changer-v1.10.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.10.0.zip
ffmpeg-wasm-media-inspector-v1.10.0.zip
ffmpeg-wasm-video-audio-extractor-v1.10.0.zip
ffmpeg-wasm-video-contact-sheet-v1.10.0.zip
ffmpeg-wasm-video-to-gif-v1.10.0.zip
ffmpeg-wasm-video-to-webp-v1.10.0.zip
ffmpeg-wasm-ffmpeg-filter-builder-single-thread-v1.10.0.zip
ffmpeg-wasm-ffmpeg-filter-builder-multi-thread-v1.10.0.zip
ffmpeg-wasm-sources-v1.10.0.tar.gz
BUILDINFO-video-compressor.txt
BUILDINFO-video-speed-changer.txt
BUILDINFO-lossless-video-cutter.txt
BUILDINFO-media-inspector.txt
BUILDINFO-video-audio-extractor.txt
BUILDINFO-video-contact-sheet.txt
BUILDINFO-video-to-gif.txt
BUILDINFO-video-to-webp.txt
BUILDINFO-ffmpeg-filter-builder-single-thread.txt
BUILDINFO-ffmpeg-filter-builder-multi-thread.txt
SHA256SUMS.txt
```

Each binary ZIP contains its generated core, runtime, manifest, profile-specific `BUILDINFO.txt`, and applicable license notices. The Video Audio Extractor bundle includes the applicable LAME notice. The corresponding-source archive contains exact FFmpeg/x264/libvpx/Opus/libwebp/Emscripten source revisions, the SHA-256-verified LAME 4.0 source archive, and the Builder recipe.

## Release verification

After the workflow succeeds:

1. open the GitHub Release for `v1.10.0`,
2. confirm `ffmpeg-wasm-video-audio-extractor-v1.10.0.zip` and `BUILDINFO-video-audio-extractor.txt` are attached,
3. confirm the asset appears in `SHA256SUMS.txt`,
4. compare the asset digest reported by GitHub with the SHA-256 in `SHA256SUMS.txt`,
5. use that exact tag, asset name, and SHA-256 in the consuming app.

Do not replace a tagged asset in place. If a release artifact must change, increment the Builder version and create a new tag.

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
