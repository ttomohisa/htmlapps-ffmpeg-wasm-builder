# Video Speed Changer Threading Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this approved task. Steps use checkbox syntax.

**Goal:** Ship separate, explicitly selected Video Speed Changer ST and MT runtimes.

**Architecture:** Reuse the existing Compressor dual-profile build/runtime contract. Limit the MT pool to eight workers, video decoding to two threads, video encoding to four and x264 lookahead to one. Keep audio and filter graphs single-threaded. Preserve the legacy ST release alias, licenses, and matching source bundle.

**Tech Stack:** Public FFmpeg libav APIs, Emscripten pthreads, JavaScript browser smoke tests, Bash release tooling.

**Spec:** Approved design: normal GitHub Pages remains ST; `/mt/` and Browser Kitty use explicit MT; unsupported MT environments show a reason and ST link. This Builder-only plan supplies dedicated runtimes for later application PRs. No merge, tag, or production release is part of this change.

## Global Constraints

- ST remains SharedArrayBuffer-free and supports portable file:// consumers.
- MT requires SharedArrayBuffer and cross-origin isolation; no silent fallback.
- Existing rate range, audio modes, inspect API, WORKERFS input and GPL licensing remain.
- Dependency pins remain unchanged; prepare Builder 1.10.6 after verifying 1.10.5 is latest.
- Do not modify ReverseClip's profile/runner or include unrelated work.

## Review Focus

- Audio decoding and both filter graphs must not oversubscribe the fixed pool.
- Legacy filenames must resolve to byte-identical ST assets.
- Tests must observe real shared memory and nested pthread workers, not only metadata.
- Cancellation must happen after core initialization, followed by a successful new run.
- Fractional/boundary rates and silent sources must retain the existing behavior in both variants.

## Task 1: Runtime thread budget

- [ ] Add failing profile/native threading tests in `tests/speed-changer-threading.test.cjs`; run with `node --test tests/speed-changer-threading.test.cjs` and observe the missing MT behavior.
- [ ] Update `profiles/video-speed-changer/profile.env` and `runners/video-speed-changer.c`, with ST defaults, bounded MT video threads and x264 lookahead, filter limit and diagnostic logs.
- [ ] Run focused and full Node/native tests.

## Task 2: Real-browser acceptance

- [ ] Add failing smoke-contract assertions for explicit threading selection, actual memory/worker observation and live-core cancellation/retry.
- [ ] Update `tests/smoke-tests/video-speed-changer.js` to check inspect, nine rates and all audio modes in each built variant.
- [ ] Run syntax and Node tests; canonical Docker/browser smoke runs in PR CI because Docker is absent locally.

## Task 3: Release compatibility

- [ ] Add failing release-contract assertions for explicit ST/MT ZIP and BUILDINFO names and legacy ST aliases.
- [ ] Update `scripts/prepare-release.sh`, `.github/workflows/release.yml`, version/check metadata, and relevant documentation.
- [ ] Run all Node/native tests, Bash syntax, whitespace checks and independent review; open a Draft PR and require real Docker/browser CI before marking ready.
