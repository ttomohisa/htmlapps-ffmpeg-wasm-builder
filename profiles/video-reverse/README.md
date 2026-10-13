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

## Experimental chunk join (new, not yet mobile-verified)

The adapter has a second, narrowly scoped entrypoint:

```text
--concat --input /last-chunk.mp4 --input /earlier-chunk.mp4 [--input ...] --output /joined.mp4
```

The caller must encode **up to 8** individually reversed chunks using the same width, height, FPS, and x264 profile, provide them in *reverse chronological order*, and feed the completed MP4 chunk bytes to the virtual filesystem. The public-libav remuxer verifies H.264 video-only stream and identical codec parameter/extradata; rejects mismatched geometry, absent timestamps and non-monotonic DTS; adjusts DTS/PTS across chunk boundaries; writes **one MP4** with no re-encoding in the join phase. Per-chunk reverse still caps buffering at 96 MiB estimated YUV420 (actual WASM heap higher). Testing must include real decoded frame order, exact frame counts, MP4 duration, VFR source and rotation; this initial smoke only verifies an actual WASM concatenation produces browser-playable video.

**Do not market this as stable 5s/10s export** until those measurements and Android/iOS real-device results exist. No audio or ping-pong in this release line.

## Frame-exact experimental contract (10-second preparation)

A caller can pass **`--frames N`** when creating each short reverse chunk. This requires `N` to be a whole number from 2 to 120 and agree with `fps × duration` within input rounding. The runner inserts `trim=end_frame=N` **after the pre-reverse FPS filter**; after encoding it reopens the output with public libavformat, counts H.264 video packets and **deletes the output on mismatch** instead of returning an MP4 with a wrong frame count.

The remux path optionally accepts **`--expected-frames N`** (2–960), counts each video packet across joined chunks and rejects an incomplete result; source codec extradata/timebases must still agree. These checks do not infer the original source frame identities. Frame-order verification remains a separate test using synthetic numbered/color-coded media.

This avoids failures found when simply expanding the app's 5-second limit: for non-frame-aligned selections, earlier unbounded FFmpeg FPS graphs sometimes added 1–2 frames. The external app must supply the exact frame plan. Android mobile performance and long-input codec compatibility remain subject to physical tests.

## Separate single-thread + multi-thread artifacts

The `video-reverse` profile now requests both `PROFILE_THREADING_VARIANTS="single-thread,multi-thread"`, following the proven FFmpeg Filter Builder pattern. The actual compiler and Chromium smoke run twice: ST from `file://`, MT from an HTTP localhost page with COOP `same-origin` and COEP `require-corp`. The generated JS/WASM binaries are **not** interchangeable; ship and hash each pair separately. `dist/video-reverse/single-thread/` and `dist/video-reverse/multi-thread/` contain their own manifest with `runtime.threading` and isolation requirements.

An MT consumer must call `BrowserFFmpeg.loadEmbedded({coreJsText,wasmBytes,threading:'multi-thread'})` and run in a cross-origin-isolated context with `SharedArrayBuffer`. The Browser Kitty MT route must supply the headers server-side. A GitHub Pages MT variant instead needs a same-origin COI Service Worker because Pages cannot set the headers; ST remains the ordinary root route and downloaded file:// option. Neither path uploads videos.

Until a permanent corresponding-source GPL release is packaged, both are experimental CI outputs only. Android/Windows MT performance is **not** established by the existence of the dual build; the old ST phone test remains separate evidence.


## Ping-pong forward chunks (v0.4 experiment)

The dedicated `video-reverse` runner now accepts `--direction forward|reverse` (default: reverse, for backward compatibility). Forward chunks follow the exact same scale, output FPS, duration-bounded trim and H.264 settings but omit the `reverse` filter. The MP4 join accepts **2–16** compatible video-only chunks in caller order, with `--expected-frames` accounting; it never concatenates MP4 bytes or silently adds audio.

For source frame indices `0...N-1`, the intended seamless-loop sequence is `0..N-1, N-2..1`, or **2N−2 frames**. The return leg omits both endpoints by selecting an exact interior input range; it does not drop encoded packets from a compressed GOP. There is no repeated frame at the turn or at the loop seam. Each individual forward/reverse chunk is at most two seconds (96MiB minimum YUV estimate) and shares the same codec parameters.

A real ST/MT browser smoke exercises a 4-frame forward section and 2-frame return, remuxed into a playable **6-frame** silent H.264 MP4. Chrome/Android physical performance, longer-VFR orientation and peak memory are still unverified. This is still an **experimental Actions build**, not a versioned GPL corresponding-source release, and does not authorize Browser Kitty integration or merging.

## v0.5.0 audio-segment experiment — NOT release ready

The dedicated `video-reverse` profile now includes *additional* AAC, MP3, Opus and FLAC decoders, native AAC encoding, and bounded `atrim / asetpts / areverse / aresample / afade` filters. A **narrow**, non-CLI `--audio-segment` mode accepts `--input`, `--output`, `--start-time`, `--duration` (0.25–10 seconds) and `--direction forward|reverse`. It requires a genuine decodable source audio stream and refuses a silent-only output. The filter trims only the requested interval, optionally reverses its decoded audio, resets time origin, and applies 8ms fade-in and fade-out ramps to reduce clicks.

**Important design boundary:** the existing Filter Builder public-libav pipeline currently also creates a disposable 16×16, 1fps H.264 stream in the intermediate MP4. It is **not** the deliverable video; future muxing must copy **only the AAC track** into the real reversed/ping-pong MP4. The zero-audio default and reverse/ping-pong video-only paths are unchanged. There is no audio-mux mode or source codec compatibility claim yet; the embedded app should not offer audio selection until end-to-end A/V mux and sync are proven. This is deliberately separated from v0.4 to allow ST/MT validation of actual AAC forwards/backwards transformation first.

The dedicated CI generates a small synthetic H.264 + AAC fixture **before** the Docker build. Its resulting `smoke-test.html` uses the fixture to assert audio inspection detects AAC, and both `--audio-segment --direction forward` and `reverse` yield actual AAC-containing MP4s. This is real WASM proof, but not yet decoded waveform correlation, A/V mux, seamless audio join, Android memory validation or a reviewed permanent GPL source-complete Release. Continue on existing Draft PR #16; do not merge.
