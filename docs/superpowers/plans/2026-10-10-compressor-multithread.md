# Video Compressor Multi-thread Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add verified Compressor ST/MT runtimes without losing MP4/WebM or portable ST.
**Architecture:** Reuse existing profile-opt-in dual-runtime machinery. Extend only Compressor codec compilation and runner threading, then integrate separate app artifacts and path-scoped Browser Kitty delivery.
**Tech Stack:** Bash, PowerShell, Docker/Emscripten, public FFmpeg C libraries, browser JavaScript.
**Spec:** ../specs/2026-10-10-compressor-multithread.md

## Global Constraints
- Eight pooled workers; video decode two, video encode four, x264 lookahead one; audio one.
- Preserve ST, both formats, timing, WORKERFS, offline embedding, and legacy ST asset alias.
- No merge, tag, manual production deployment, secret or account changes.

## Review Focus
- VP9 multithreading must actually be enabled, not merely expose SharedArrayBuffer.
- Pool exhaustion must not hang repeated H.264/VP9 runs.
- ST asset consumers must keep working under the legacy release name.
- Non-isolated MT must fail clearly before loading the engine.
- Cancel/retry must dispose nested workers and preserve UI state.

## Tasks
1. Builder runtime: add failing source/packaging regressions and extend real browser smoke tests; then implement profile opt-in, codec build flags and runner thread budgets. Run Node regression suites, Bash syntax checks, official source checks and actual Docker/browser CI. Review complete diff and open English draft PR.
2. App integration: add failing variant/build/guard/lifecycle tests; produce ST/MT embedded artifacts from one verified Builder version, retain safe workers/disposal and apply two solid backgrounds. Update bilingual help, architecture, metadata and wrappers; run aggregate checks and native synthetic MP4/WebM cancel/retry.
3. Distribution: test selected MT source and exact isolation path, update only Compressor config/headers/docs, run site validation and preview check. Merge order remains owner-controlled, with release availability checked before switching the live consumer.
