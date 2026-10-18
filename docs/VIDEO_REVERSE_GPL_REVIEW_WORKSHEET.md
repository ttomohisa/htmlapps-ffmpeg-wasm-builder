# video-reverse 1.10.6 — corresponding-source review worksheet

**Prepared 2026-10-19. REVIEW ONLY. No legal sign-off or publication authorization.**

This separates technical evidence from the legal/completeness decisions remaining
before a proposed profile-specific GitHub Release. It does not approve a tag,
Release, merge, or Reverse Clip runtime promotion.

## Source and binary identity

| Item | Audited reference |
| --- | --- |
| Exact compiled Builder source | 8e60da7cf9de644ce3f6cce9c007b17e6a385c53 |
| Builder version recorded in binary | 1.10.6 |
| Proposed profile-specific Release tag | video-reverse-v1.10.6 — NOT CREATED |
| ST binary ZIP SHA-256 | 9087bf4d03b29d141e78c93e9de62a237f2d408f24b43caeef26072e6d3b21b0 |
| MT binary ZIP SHA-256 | b577be491754ecfa96d0b86021184b82b2fc280f8881bd73cb5d52d83914d3bd |
| Corresponding-source TAR SHA-256 | 9ceb47a5f46fb3484f8b1f6a8fa37e6d0dc4144aa09d78939ac9d23e8ab5ce92 |
| Separate GPL/x264 notice SHA-256 | e011d66e3451dbf5ffa5f67f26636749664a69d285a3cfc665a71e36635e15f3 |
| Original binary metadata label | Release tag: v1.10.6 |
| Latest review-only Actions artifact | 12145680508 (expires 2026-11-17 17:48:11 UTC) |
| Latest review-only CI | [#39151222248](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39151222248) — PASS |

The already-existing Builder-wide v1.10.6 Release does not ship video-reverse.
There is no profile Release at the proposed tag.

## 2026-10-19 — exact compiled-source recipe audit

A new [offline archived-source verifier](../scripts/verify-video-reverse-compiled-source.py) compares the actual bytes of **17 selected Builder recipe files** inside the original SHA-256-pinned corresponding-source tarball with independent Git blob IDs read from the exact compiled Builder commit `8e60da7cf9de644ce3f6cce9c007b17e6a385c53`. The set includes the Dockerfile, build scripts, version pins, FFmpeg/x264 build helpers, video-reverse C and pthread JS runner, profile flags, shared browser loader and original notices. The verifier rejects missing, duplicated, truncated, symlink or modified selected inputs and checks declared FFmpeg/x264/Emscripten revisions.

[Actual retained-archive CI #39151222248](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39151222248) **PASS**: logs explicitly report `PASS 17 exact compiled-commit Builder recipe Git blob IDs` while restaging the unchanged ST/MT/source package. Updated review-only candidate **12145680508** expires **2026-11-17T17:48:11Z**. [Small independent six-test identity CI #39151202156](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39151202156) **PASS**. These results strengthen source-to-recipe identity evidence without recompiling binaries or attesting complete GPL corresponding source.

**Still open:** full corresponding-source/licensing inspection, reproducible/toolchain compilation, Docker image digest availability, durable public distribution and owner sign-off. The stored artifact remains temporary.

## Technical evidence already verified

- [x] Independently pinned full ST and MT binary ZIP digests.
- [x] Independently compared 17 critical archived Builder source files against their exact compiled Git commit blobs on the real retained artifact.
- [x] Original core JS/WASM, shared browser loader and threading manifests
  match the Reverse Clip runtime lock.
- [x] The 26,555-entry source bundle includes FFmpeg, x264, Emscripten,
  Builder build scripts, dedicated runner, profile settings and license files.
- [x] Each original binary reports GPL-2.0-or-later with x264 linked.
  The old embedded generic notice omits video-reverse by name; the
  separately audited GPL notice addendum covers this discrepancy.
- [x] Review-only archives can be refreshed from still-retained candidates
  after original artifact expiry without modifying pinned bytes.
- [x] Post-publication unauthenticated asset/URL/tag/hash verifier and
  offline rejection-gate CI prepared; no actual Release tested.

These checks establish matching files and source *presence*, not legal
corresponding-source completeness or an independently reproduced build.

## Required independent review — all still OPEN

- [ ] **Corresponding source completeness:** Inspect the actual archive,
  the source code and required patches, generated inputs and scripts that
  control compilation and installation for both ST and MT. Check
  Builder build.sh, scripts/build-ffmpeg.sh, profile flags and
  the specialized video-reverse C/JS glue.
- [ ] **Exact build provenance:** Confirm availability of the toolchain,
  compiler flags, dependencies and build procedure. The repository records
  the emscripten/emsdk:6.0.6 container tag, but a container image digest
  has not been established in this review. A tagged image alone is
  not evidence of a bit-identical reproducible build.
- [ ] **Licensing/notice assessment:** Independently assess actual FFmpeg,
  x264 and Emscripten terms, GPL corresponding-source obligations,
  license files and whether the separate notice is sufficient.
  This requires qualified review when appropriate.
- [ ] **Naming disclosure:** Approve or revise the profile-specific tag
  while explicitly acknowledging the original embedded v1.10.6 label.
  Do not change the original checked binary/source bytes or overwrite
  the existing Builder-wide v1.10.6 Release.
- [ ] **Long-term public availability:** Decide how all seven Release
  assets (ST, MT, source, both BUILDINFO, SHA256SUMS, GPL notice) stay
  available for the applicable period. GitHub Actions artifacts expire.
- [ ] **Owner publication approval:** Explicit approval of the exact tag,
  source commit, seven assets and Release notes is required before publish.

## Post-publication procedure — do not execute yet

1. Only after review and express approval, publish the seven original
   assets with the profile-specific tag resolving to the compiled source.
2. From an unauthenticated environment run:

    python3 scripts/verify-video-reverse-public-release.py --evidence-json video-reverse-public-release-proof.json

3. Retain the public URL/hash report. Independently establish long-term
   public availability; the script reports if GitHub API does not attest
   immutable publication. This check does not approve GPL compliance.
4. Separately approve the Reverse Clip GitHub Release importer lock,
   rebuild and verify ST single HTML and isolated hosted MT, and decide G3.
   The user alone may merge the Draft PRs.

## Review sign-offs — deliberately blank

- Corresponding-source/license reviewer, date, decision:
- Build provenance/reproduction reviewer, date, decision:
- Owner release-naming and publication approval, date:
- Public download/hash evidence report:
- Reverse Clip final distribution QA and G3 decision:

**State: source/legal review PENDING, reproducible build NOT ESTABLISHED,
public Release NOT PUBLISHED, G3 NOT PASSED.**

See [publication handoff](VIDEO_REVERSE_PUBLICATION_HANDOFF.md),
[source audit](VIDEO_REVERSE_SOURCE_RELEASE.md),
and [GPL addendum](VIDEO_REVERSE_LICENSE_ADDENDUM.md).
