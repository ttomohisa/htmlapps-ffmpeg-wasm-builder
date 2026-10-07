# Final-frame Duration Implementation Plan

> **For agentic workers:** Use red-green development, then independent review of the complete branch.

**Goal:** Preserve the last encoded video frame's display interval in Compressor MP4 and GIF output.

**Architecture:** Carry frame duration across the public libav encoder boundary in encoder time-base units. Validate actual encoded samples and animation delays rather than aggregate frame-rate metadata.

**Tech Stack:** C, pinned FFmpeg/Emscripten, JavaScript WASM and browser tests.

**Spec:** The approved bounded upstream fix covers runners, regression tests, and an English Draft PR. It excludes releases, tags, packages, merging, and consuming-app updates.

## Constraints

- Keep dependency pins, single-threaded runtime, audio behavior and public runner flags unchanged.
- Do not stamp a guessed constant duration onto Original/VFR output.
- Keep generated dist artifacts out of Git.
- Canonical Docker builds and real browser smoke tests must pass before calling runtime verification complete.

## Review focus

- Original/VFR intervals must survive rescaling without truncation or CFR substitution.
- Single-frame output must have a positive terminal interval.
- Fractional CFR and GIF centisecond quantization require per-sample assertions.
- Zero/unknown durations must not be rescaled as a known positive value.
- B-frame ordering and AAC must not hide a shortened video track; VP9/WebP controls must stay valid.

## Steps

- [x] Add bounded structural MP4/GIF/WebP timing readers, synthetic fixtures and actual-WASM tests; run unchanged released cores to demonstrate red.
- [x] Enable the frame-duration encoder contract, rescale valid video frame duration with PTS, and handle only justified timing fallbacks.
- [x] Extend browser smoke tests to assert encoded timing, while keeping existing codec/audio/rotation checks.
- [x] Run local repository checks and parser/unit tests. Review the complete diff independently.
- [ ] Create an English Draft PR on a dedicated branch, verify remote tree, and monitor its existing canonical Docker/browser CI to a terminal result.
