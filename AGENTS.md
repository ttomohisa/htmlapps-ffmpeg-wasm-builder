# AGENTS.md

## Product goal

Self-build current FFmpeg for focused browser apps without depending on prebuilt `@ffmpeg/ffmpeg` binaries.

## Required architecture

- Public `libav*` runner only; do not link the upstream `ffmpeg` CLI.
- Existing profiles stay single-threaded unless their profile metadata explicitly opts into a multi-thread variant.
- `ffmpeg-filter-builder` is the first dual-runtime profile and must build both single-thread and pthread variants.
- Single-thread variants remain SharedArrayBuffer-free and keep direct `file://` single-HTML support.
- Multi-thread variants may require SharedArrayBuffer + cross-origin isolation and must never silently replace the single-thread artifact.
- Long processing runs in a normal Web Worker; pthread builds may create nested Emscripten workers inside it.

## Non-negotiable rules

- Keep FFmpeg/Emscripten/x264 pins only in `versions.env`.
- Do not silently add pthreads to an existing single-thread profile; pthreads require an explicit profile opt-in and a separate artifact.
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
