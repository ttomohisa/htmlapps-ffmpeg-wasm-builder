# Releasing

`main` builds and smoke-tests every profile listed in `.github/workflows/build.yml`. Do not tag a release until all matrix jobs are green.

For v1.5.0:

```text
git tag -a v1.5.0 -m "FFmpeg WASM Builder v1.5.0"
git push origin v1.5.0
```

The tag workflow verifies that the tag matches `BUILDER_VERSION`, rebuilds all current release profiles, runs their real browser smoke tests, then publishes:

```text
ffmpeg-wasm-video-compressor-v1.5.0.zip
ffmpeg-wasm-lossless-video-cutter-v1.5.0.zip
ffmpeg-wasm-media-inspector-v1.5.0.zip
ffmpeg-wasm-video-contact-sheet-v1.5.0.zip
ffmpeg-wasm-video-to-gif-v1.5.0.zip
ffmpeg-wasm-video-to-webp-v1.5.0.zip
ffmpeg-wasm-sources-v1.5.0.tar.gz
BUILDINFO-video-compressor.txt
BUILDINFO-lossless-video-cutter.txt
BUILDINFO-media-inspector.txt
BUILDINFO-video-contact-sheet.txt
BUILDINFO-video-to-gif.txt
BUILDINFO-video-to-webp.txt
SHA256SUMS.txt
```

Each binary ZIP contains its generated core, runtime, manifest, profile-specific `BUILDINFO.txt`, and applicable license notices. The corresponding-source archive contains exact FFmpeg/x264/libwebp/Emscripten source revisions plus the Builder recipe.
