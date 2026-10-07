# Smoke tests

`tests/fixtures/smoke-input.mp4` is a tiny H.264 + AAC MP4 shared by the current profiles.

The generic `tests/smoke-test.template.html` embeds the generated JS/Wasm and injects `tests/smoke-tests/<profile>.js`.

- `video-compressor.js` performs a real transcode and validates MP4/H.264/AAC output.
- `lossless-video-cutter.js` performs a real stream-copy cut, validates MP4/H.264/AAC preservation, verifies output shrank, and checks the runner's keyframe-aligned start report.

A build is not considered successful until the selected profile passes in a real headless browser.

## Final-frame timing regressions

The Compressor, GIF, and WebP browser smoke tests also run the shared
`timing-regressions.js` cases with the synthetic inputs described in
`fixtures/TIMING.md`. These inspect encoded MP4 samples/edit coverage, GIF GCE
intervals, and WebP ANMF intervals; cadence and endpoint checks are separate.
The existing Compressor smoke continues testing AAC and Opus plus rotation.

For an additional Node/MEMFS run against actual generated core bytes:

```sh
node --test tests/timing-readers.test.cjs
node tests/run-timing-regressions.cjs video-compressor dist/video-compressor
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
