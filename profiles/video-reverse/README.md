# Video Reverse — Experimental FFmpeg WASM Profile

**Status: source-only PoC, not built or tested in WASM. Not a tagged release.**

Reverse Clip received one Android Chrome-family diagnostic (2026-10-11): native MP4 preview, empty WASM, Blob Worker and 0.2s video seek (43ms) passed. It did **not** prove actual FFmpeg reverse conversion. This profile is the next isolated technical experiment.

## Scope

- Public-libav single-thread runner: `runners/video-reverse.c` reuses `ffmpeg-filter-builder.c` instead of forking a 65 KB codec/transcode implementation.
- Input is WORKERFS `File`/ `Blob`; one video stream; output a single reversed **H.264 MP4 chunk without audio**.
- Accepts `--input PATH --output PATH --start-time S --duration S --width EVEN --height EVEN --fps INTEGER [--crf N]`.
- Requires a bounded 0.1–2.0s range and estimated YUV420p frame buffer no more than 96 MiB. This is a *minimum estimate*, not an actual hard heap limit.
- FFmpeg filter chain: trim + normalize PTS + autorotate → scale → fps → reverse → normalize PTS → final fps → yuv420p/encode. **Scale and frame-rate limit happen before reverse**.
- No arbitrary filter strings, URLs, cross-origin requests, general FFmpeg CLI arguments or runtime CDN. No shared profile, public release catalog or upstream version pins changed.

## Build (not yet performed)

From a full checkout with Docker Desktop/Linux engine, Emscripten/build dependencies and a browser:

```bash
./build.sh video-reverse
```

The build is designed to emit `dist/video-reverse/{ffmpeg.js,ffmpeg.wasm,manifest.json,smoke-test.html}` and to invoke `tests/smoke-tests/video-reverse.js` in a browser. These outputs **do not exist** until a real build succeeds; **do not ship** this profile by name alone. The general nine-profile Actions matrix is deliberately unchanged to avoid redundant costly builds just for source staging.

## Outstanding G1 work

1. Build and smoke-test the actual WASM. Correct any missing filter linkage, segment timing or decode issues.
2. Generate explicit indexed-frame fixtures; compare complete decoded output frame order and catch dropped/duplicated boundary frames, including VFR and rotated MOV.
3. Verify real mobile Chrome/Safari 720p/5s, chunk memory and conversion speed, MP4 playback and saving.
4. Introduce **reverse-order stitching** with matched timebases, PTS, H.264 parameters, and no duplicate turnaround frames. This adapter itself emits only one chunk.
5. Determine whether iPhone HEVC decode and display-matrix orientation are correct. Then separately consider `areverse` with audio buffering limits.
6. Add embedded, pinned runtime assets to Reverse Clip only after successful Builder versioned artifacts and license/corresponding source validation.

This experiment does not pass Reverse Clip's Gate G1. It is an intermediate engineering artifact.
