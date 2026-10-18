# Smoke tests

`tests/fixtures/smoke-input.mp4` is a tiny H.264 + AAC MP4 shared by the current profiles.

The generic `tests/smoke-test.template.html` embeds the generated JS/Wasm and injects `tests/smoke-tests/<profile>.js`.

- `video-compressor.js` performs real H.264/AAC MP4 and VP9/Opus WebM transcodes in both threading variants, and checks rotation and timing.
- `video-speed-changer.js` runs both explicit threading variants, observes actual shared WASM memory and nested pthread Worker construction, checks inspect and bounded codec threads, cancels after encoder startup and retries successfully, then covers all nine rates plus pitch-shift/audio-removal boundary rates and silent input.
- `lossless-video-cutter.js` performs a real stream-copy cut, validates MP4/H.264/AAC preservation, verifies output shrank, and checks the runner's keyframe-aligned start report.

A build is not considered successful until the selected profile passes in a real headless browser.

## Contact Sheet sample partitions

`contact-sheet-partition.test.cjs` checks the backward-compatible runtime argument
helper and compiles the actual C argument parser plus partition helpers when `cc`
is available. The browser smoke uses actual generated Wasm and WORKERFS to compare
one full run with two concurrent partitions for all 12/24/48 choices. Every RGB
output byte, target/actual timestamp, global sample index and geometry field must
match, and unowned cells must remain black. This is correctness coverage, not a
benchmark or a claim about browser peak memory.

`contact-sheet-resolution.test.cjs` exercises the actual native parser/geometry
with stubbed FFmpeg metadata access, plus the JS argument helper. It covers
960/840/660 px, invalid bounds, portrait rotation, all legacy <=640 px grids,
empty last-row allocation and the 25-million-pixel guard. Real-browser smoke
extracts all three high-resolution presets, compares exact sampling timestamps,
checks every tile for decoded content and proves direct C rejection of 961 px
and an oversized sheet. The 25M limit is per PPM, not aggregate browser memory.

## Final-frame timing regressions

The Compressor, GIF, and WebP browser smoke tests also run the shared
`timing-regressions.js` cases with the synthetic inputs described in
`fixtures/TIMING.md`. These inspect encoded MP4 samples/edit coverage, GIF GCE
intervals, and WebP ANMF intervals; cadence and endpoint checks are separate.
The existing Compressor smoke continues testing AAC and Opus plus rotation.

For an additional Node/MEMFS run against actual generated core bytes:

```sh
node --test tests/timing-readers.test.cjs
node tests/run-timing-regressions.cjs video-compressor dist/video-compressor/single-thread
node tests/run-timing-regressions.cjs video-to-gif dist/video-to-gif
node tests/run-timing-regressions.cjs video-to-webp dist/video-to-webp
```

Node coverage is supplemental. It does not replace `./build.sh <profile>` and the
canonical real-browser/WORKERFS smoke test. Parser unit tests use synthetic valid
and malformed structures to ensure missing timing data cannot silently pass.

## Filter Builder final-frame timing

Both canonical Filter Builder browser variants run `filter-builder-timing-regressions.js`
using actual embedded cores and WORKERFS files. Its 40 cases cover single-input and
multi-input full grayscale, half-open previews (0..3 and 1..4), scale, 15/12.5 fps,
VFR and a distinct final VFR interval, one-frame video, 1.5x/0.5x/chained speed,
speed followed by fps, matching atempo, and mixed-timebase/secondary-speed overlays.
Two of those cases check that an unknown setpts expression is accepted with an
explicit unknown-duration warning; they make no exact-duration claim. A final fps
filter is tested separately as an explicit cadence after an unknown expression.

Positive coded sample intervals, exact PTS, geometry, media duration (CFR), and
edit-list presentation coverage (including reordered VFR) are mandatory. Browser
VideoDecoder verifies actual H.264 coded-sample decode counts and grayscale pixels;
this alone is not a normal MP4 demux/playback count, so it never substitutes for the
structural interval/edit checks. Audio decoding checks 440 Hz and range coverage,
with 65 ms allowed for AAC/atempo boundary delay. Existing channel behavior is
preserved: single-input mono, multi-input stereo, both 48 kHz. The multi-input
runner already converted mono to stereo before this timing repair.

The supplemental Node run requires native `ffprobe` and `ffmpeg`, uses ordinary
MP4 demuxing (no `-ignore_editlist`) to count normally decoded frames, and checks
pixel content/source alignment and audio separately. Preview alignment uses a
calibrated pixel-error bound of 4 (correct 1-second frames <=1.51; wrong 0/2-second
frames >=8.50), plus a source-zero negative control through the same assertion.
Browser AAC sample rate is read from the encoded MP4 sample description, not the
AudioContext resampling rate. An optional evidence directory
retains every MP4, core log, ffprobe report, and executed argument list:

```sh
node tests/run-filter-builder-timing-regressions.cjs dist/ffmpeg-filter-builder/single-thread ./filter-timing-evidence
cc -Wall -Wextra -Werror -std=c11 tests/filter-builder-duration-scale.test.c -lm -o /tmp/filter-duration-test
/tmp/filter-duration-test
```

Every matrix cell runs even if an earlier one fails. The new tests do not change
existing app oracles, profile/dependency pins, or the older timing readers. Node
and native-runner evidence supplement the required Docker build and browser ST/MT
runs; they do not establish a released Wasm artifact is fixed.

## Speed Changer threading

`node --test tests/speed-changer-threading.test.cjs` checks profile/release contracts and compiles the actual runner's decoder/encoder/filter thread assignments for ST and MT. The native test uses minimal FFmpeg type stubs; it is not a real transcode. Canonical `./build.sh video-speed-changer` remains mandatory for both actual Wasm cores and real browser/WORKERFS behavior.
