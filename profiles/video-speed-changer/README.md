# video-speed-changer profile

Compact public-libav profile for Browser Kitty Video Speed Changer.

- Changes video timestamps with `setpts`.
- Changes audio tempo with `atempo` when preserving pitch.
- Uses `asetrate` + `aresample` when pitch should change together with speed.
- Can drop the audio stream entirely.
- Chains `atempo` stages so the public API accepts 0.25x to 4.00x.
- Autorotates display-matrix video before H.264 encoding.
- Outputs H.264 + AAC MP4, or H.264-only MP4 when audio is removed.
- Uses WORKERFS for browser `File` / `Blob` input.
- Does not expose arbitrary FFmpeg CLI arguments.

Build note: `buffer`, `buffersink`, `abuffer`, and `abuffersink` are public libavfilter graph endpoints that FFmpeg registers manually. They must not be passed through `--enable-filter=` or checked as `CONFIG_*_FILTER` components.

`v0.4.0` of the consuming app uses the full 0.25x to 4.00x range and all three audio modes through the same narrow public API.
