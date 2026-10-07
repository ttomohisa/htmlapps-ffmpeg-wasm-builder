# Releasing

`main` builds and smoke-tests every profile listed in `.github/workflows/build.yml`. Do not tag a release until all matrix jobs are green.

v1.10.2 is a patch release for the merged Filter Builder frame-duration and half-open preview repair in both ST and MT runtimes. It keeps the v1.10.1 profile set and dependency pins.

After the release-preparation PR is merged and every matrix job for that exact `main` commit succeeds, create the new tag:

```text
git switch main
git pull --ff-only
git tag -a v1.10.2 -m "FFmpeg WASM Builder v1.10.2"
git push origin v1.10.2
```

The tag workflow verifies that the tag matches `BUILDER_VERSION`, rebuilds all current release profiles, and runs their real-browser smoke tests before publishing. `ffmpeg-filter-builder` still ships both single-thread and COOP/COEP-hosted multi-thread variants.

v1.10.2 publishes:

```text
ffmpeg-wasm-video-compressor-v1.10.2.zip
ffmpeg-wasm-video-speed-changer-v1.10.2.zip
ffmpeg-wasm-lossless-video-cutter-v1.10.2.zip
ffmpeg-wasm-media-inspector-v1.10.2.zip
ffmpeg-wasm-video-audio-extractor-v1.10.2.zip
ffmpeg-wasm-video-contact-sheet-v1.10.2.zip
ffmpeg-wasm-video-to-gif-v1.10.2.zip
ffmpeg-wasm-video-to-webp-v1.10.2.zip
ffmpeg-wasm-ffmpeg-filter-builder-single-thread-v1.10.2.zip
ffmpeg-wasm-ffmpeg-filter-builder-multi-thread-v1.10.2.zip
ffmpeg-wasm-sources-v1.10.2.tar.gz
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

1. open the GitHub Release for `v1.10.2`,
2. confirm all 22 assets listed above are attached, including all 10 profile ZIPs and the corresponding-source archive,
3. verify downloaded asset SHA-256 values against `SHA256SUMS.txt` and GitHub asset digests,
4. confirm each manifest and BUILDINFO identifies Builder 1.10.2 and the intended profile/threading variant,
5. confirm binary bundles contain their applicable license notices and the source archive contains the pinned sources and matching Builder recipe,
6. use that exact tag, asset name, and verified SHA-256 in the consuming app.

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
