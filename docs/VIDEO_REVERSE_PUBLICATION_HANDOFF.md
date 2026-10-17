# Reverse Clip — profile-scoped FFmpeg WASM publication handoff

**Prepared 2026-10-17. Status: REVIEW ONLY. No release/tag/merge/production distribution has been authorized or performed.**

## Why not publish into existing v1.10.6?

The Builder-wide [v1.10.6](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/releases/tag/v1.10.6) already exists, and its normal release assets **do not contain video-reverse**. Do not overwrite that published tag, replace an existing asset, or quietly label binaries compiled as Builder 1.10.6 as Builder 1.10.7.

A **proposed distinct profile-scoped tag** is `video-reverse-v1.10.6` (versioned profile release, not a newly compiled Builder-wide version). As of 2026-10-17, that tag and Release do **not** exist. Before publication, the owner must approve this naming and the difference between the original binary BUILDINFO's `Release tag: v1.10.6` and the profile-specific GitHub Release tag. Make the distinction explicit in public release notes if approved; do not silently change build metadata without updating its checksums and review.

For the original matching compiled source, use `8e60da7cf9de644ce3f6cce9c007b17e6a385c53`. **The eventual tag must resolve to the exact compiled source commit**, not the current PR head (which contains later documentation and staging scripts). This retains verifiable source-to-binary provenance.

## Staged evidence — still temporary

[Review staging CI #39004330416](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39004330416) was **SUCCESS** and produced temporary Actions artifact **12089984511**, scheduled to expire **2026-11-16T04:55:02Z**. It used the existing source-complete 1.10.6 candidate ZIP (artifact 11920042823) and did **not** rebuild FFmpeg.

This stage invokes the previously independent `scripts/verify-video-reverse-review-bundle.py` and copies the **exact existing bytes** into `release-candidate/`, then reruns `sha256sum -c SHA256SUMS.txt`. Files prepared for independent review are:

| Asset | Meaning |
| --- | --- |
| `ffmpeg-wasm-video-reverse-single-thread-v1.10.6.zip` | ST FFmpeg JS/WASM + loader + notices |
| `ffmpeg-wasm-video-reverse-multi-thread-v1.10.6.zip` | MT/pthread FFmpeg JS/WASM + loader + notices |
| `ffmpeg-wasm-sources-v1.10.6.tar.gz` | matching source archive and Builder recipe |
| `BUILDINFO-video-reverse-single-thread.txt` | original ST build metadata |
| `BUILDINFO-video-reverse-multi-thread.txt` | original MT build metadata |
| `SHA256SUMS.txt` | original checksums for all five payloads |
| `VIDEO_REVERSE_PROMOTION_MANIFEST.json` | additional review-only provenance/status manifest |
| `VIDEO_REVERSE_LICENSE_ADDENDUM.md` | separately SHA-256-pinned supplemental GPL/x264 profile disclosure (added in latest staging) |

The review package is intentionally **not** a permanent public source delivery mechanism. It is a time-limited backup and release-preparation input.

## Legacy embedded notice gap found and separately disclosed

Independent examination of the original audited ST and MT ZIP contents
confirmed that their embedded `THIRD_PARTY_NOTICES.md` lists older
x264/GPL profiles but **omits `video-reverse` by name**. The original
binary's `BUILDINFO.txt` correctly says `video-reverse`,
`GPL-2.0-or-later` and `x264 linked into this profile: yes`. Therefore
the gap is **specificity of the general notice**, not absence of the
upstream GPL and x264 license text inside the binary.

To preserve all previously audited binary/source/checksum bytes, added
[separate license addendum](VIDEO_REVERSE_LICENSE_ADDENDUM.md) explaining
that GPL/x264 classification, the Builder tag name discrepancy and
matching-source requirements. Updated the Builder Draft branch's general
`THIRD_PARTY_NOTICES.md` for **future builds only**; it does not rewrite
the reviewed archives.

The latest addendum-enforcing [staging CI #39006344474](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39006344474)
**SUCCESS**. The refreshed review-only artifact **12090973204**, expires
**2026-11-16T05:28:00Z**. In addition to the five original checked
payloads and original `SHA256SUMS.txt`, its provenance manifest
independently SHA-256-pins `VIDEO_REVERSE_LICENSE_ADDENDUM.md`.
CI verifies that legacy embedded notices really omit the profile, and
that each binary's original BUILDINFO positively identifies
`video-reverse`, GPL and x264.

**Still not published, not legally approved and not an independently
reproduced compilation.** Include the addendum *as a separately
downloadable Release asset* and explicitly link it in proposed release
notes, or approve a replacement independently rebuilt/reviewed
distribution with the corrected notice inside its new binaries.

## Required explicit sign-offs before a public Release

1. **Corresponding-source/licensing review:** verify applicable FFmpeg/x264 GPL terms and the actual source archive's completeness (including all scripts/patches needed for this WASM build), notices and upstream licenses. The automated source-*presence* check is not a legal opinion or proof of reproducible compilation.
2. **Source-to-binary/reproduction review:** confirm or document any inability to independently rebuild and compare; do not claim bit-identical reproduction from the mere presence of source trees.
3. **Naming and permanence:** approve the distinct profile-specific versioned tag; create/publish it only with owner authorization, and keep binary+source assets available through the required period.
4. **Publish unchanged audited assets plus disclosure:** verify all five SHA-256 values, the added manifest's provenance and the separate GPL license addendum SHA-256. Explicitly link the addendum in public notes, and verify publicly retrievable direct URLs from a clean environment (not an authenticated Actions download). Refuse hash mismatch or overwritten assets.
5. **Consumer promotion:** only after all public assets are checked, change Reverse Clip's `reverse-runtime.lock.json` from `temporary-not-release` to `github-release-st-mt`, filling in exact `builderTag`, both `releaseAsset` and `releaseArchiveSha256`, and exact `correspondingSourceAsset`/`correspondingSourceSha256`, plus `supplementalNoticeAsset`/`supplementalNoticeSha256`. The consumer importer is prepared to verify **all three** public source, binary and supplemental notice hashes.
6. **Final app QA:** rebuild the exact ST/MT single HTML files, verify desktop/browser media integrity, CSP/no user media upload and live isolated Android-targeted MT hosting. The Android ST/MT four-action smoke has already passed as **owner-reported feedback** on the development build; avoid mislabeling it as independent validation of any changed final release binary.

**Gate decisions:** historical original G2 **NOT PASSED**, release G3 **NOT PASSED**, and publication remains **blocked** pending the items above. Browser Kitty site and PR merges are owner-controlled separate work.

## What has been implemented (safe preparation only)

- `scripts/verify-video-reverse-review-bundle.py`: offline byte/source-presence audit.
- `scripts/stage-video-reverse-promotion.py`: byte-preserving extraction into release-candidate directory with proposed tag and manifest (requires the original audited ZIP).
- `.github/workflows/stage-video-reverse-promotion.yml`: short, limited-cost workflow, no Docker or publication, retention 30 days.
- The regular Builder `.github/workflows/release.yml` remains unchanged and still does **not** publish this experimental profile.

No automatic action will create a tag, GitHub Release or merge.
