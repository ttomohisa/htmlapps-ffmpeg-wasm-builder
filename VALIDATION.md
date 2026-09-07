# Validation

Repository validation checks that:

- the upstream FFmpeg CLI remains disabled;
- existing profiles remain single-thread unless they explicitly opt in;
- `ffmpeg-filter-builder` declares both ST and MT variants;
- ST disables pthreads and requires no pthread Worker program;
- MT enables FFmpeg/Emscripten pthreads, reuses `ffmpeg.js` as the pthread Worker program through `mainScriptUrlOrBlob`, and records SharedArrayBuffer / cross-origin-isolation requirements in manifest schema 8;
- the browser runtime passes embedded `ffmpeg.js` to Emscripten through `mainScriptUrlOrBlob` without a separate pthread worker asset;
- profile-required FFmpeg components exist;
- release/CI include both Filter Builder variants.

The decisive compatibility check remains the real browser smoke test. Existing profiles and Filter Builder ST use the portable path. Filter Builder MT is served from a temporary local HTTP server that adds COOP/COEP headers, then verifies cross-origin isolation and executes a real `scale=160:90` filter.

Static validation alone must never be described as a successful full FFmpeg build.
