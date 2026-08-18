# Smoke tests

`tests/fixtures/smoke-input.mp4` is a tiny H.264 + AAC MP4 shared by the current profiles.

The generic `tests/smoke-test.template.html` embeds the generated JS/Wasm and injects `tests/smoke-tests/<profile>.js`.

- `video-compressor.js` performs a real transcode and validates MP4/H.264/AAC output.
- `lossless-video-cutter.js` performs a real stream-copy cut, validates MP4/H.264/AAC preservation, verifies output shrank, and checks the runner's keyframe-aligned start report.

A build is not considered successful until the selected profile passes in a real headless browser.
