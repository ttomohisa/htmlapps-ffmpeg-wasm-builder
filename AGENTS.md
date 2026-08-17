# AGENTS.md

## Product goal

Self-build current FFmpeg for focused browser apps without depending on prebuilt `@ffmpeg/ffmpeg` binaries.

## Required architecture

- Public `libav*` runner only; do not link the upstream `ffmpeg` CLI.
- FFmpeg and x264 stay single-threaded.
- No SharedArrayBuffer / cross-origin isolation dependency.
- Long processing runs in a normal Web Worker.
- Direct `file://` single-HTML use remains supported.

## Non-negotiable rules

- Keep FFmpeg/Emscripten/x264 pins only in `versions.env`.
- Do not silently add pthreads.
- Do not use FFmpeg private/internal APIs unless explicitly documented and unavoidable.
- Features require both profile components and runner implementation.
- Keep network protocols disabled by default.
- Keep generated `dist/` out of git.
- Keep manifest/version metadata.
- Keep exact Emscripten source ref/commit alongside the Docker toolchain version.
- Tagged releases must publish corresponding FFmpeg/x264/Emscripten source and upstream license files beside the binary bundle.
- Never label generated FFmpeg/x264 Wasm as MIT.
- x264-enabled generated binaries are GPL-covered.
- Keep the browser smoke test as a real transcode, not a file-existence or `--version` check.

## Before handoff

Run repository checks. A release/update is not considered validated until a real Docker build and browser smoke test pass.
