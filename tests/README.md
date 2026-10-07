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
