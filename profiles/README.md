# Build profiles

Each app profile is a small build contract:

```text
profiles/<profile>/
├─ profile.env       # required config/link metadata + manifest capabilities
├─ ffmpeg.flags      # FFmpeg configure flags
├─ README.md         # profile behavior/limitations
└─ single-html/      # optional Builder demo shell

runners/<profile>.c

tests/smoke-tests/<profile>.js
```

`ffmpeg.flags` starts from `--disable-everything` and enables only the components needed by that tool. `profile.env` tells the linker which FFmpeg static libraries are actually required and whether x264 or libwebp belongs in the final Wasm. The C runner exposes the operation; the profile-specific smoke test must exercise it in a real browser.

Current release profiles include `video-compressor`, `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, and `video-to-webp`. The two animation profiles share a public-libav runner: GIF uses a two-pass palette pipeline, while WebP links pinned libwebp only in its own build. WORKERFS is used for large browser File/Blob inputs where random access is useful.
