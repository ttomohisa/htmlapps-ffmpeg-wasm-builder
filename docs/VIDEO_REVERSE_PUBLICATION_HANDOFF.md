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

## 2026-10-17 — ready-to-review app lock proposal generated (NOT activatable)

The updated [promotion CI #39008016046](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39008016046) **SUCCESS** independently verified the same five unchanged Builder v1.10.6 ST/MT/source/BUILDINFO payloads and separate GPL notice. It generated **`REVERSE_CLIP_RELEASE_LOCK_PROPOSAL.json`** from the actual staged files and inner `ffmpeg.js`/`ffmpeg.wasm` hashes instead of copying proposed SHA values by hand.

- New review-only Actions artifact: **12091513054**, expires **2026-11-16T05:55:36Z**. The previous stages remain historical audit evidence; this is the latest verified handoff package.
- Generator: [`scripts/propose-video-reverse-app-lock.py`](../scripts/propose-video-reverse-app-lock.py). It validates staged bytes and returns a document containing `proposedLock` with correct Builder revision, proposed profile-specific tag, ST/MT binary ZIP hashes, corresponding-source SHA, GPL addendum SHA, browser wrapper SHA, and existing `pingpong`/`audio-mux` feature flags.
- **The proposal intentionally uses `kind: review-only-proposed-github-release-st-mt` and `status: review-only-not-published`.** These values are not accepted by Reverse Clip's production importer. The manifest explicitly records `releasePublished: false`, `legalApprovalRecorded: false`, and `sourceBinaryReproductionEstablished: false`.
- The app importer now additionally requires an explicit **`status: release-approved`** for the actual `kind: github-release-st-mt` path. The app's *current* `reverse-runtime.lock.json` remains the old experimental temporary-artifact configuration.
- In Windows CI, actual PowerShell importer tests reject: (1) the staging-only kind, (2) a public-shaped kind with unapproved status, and (3) an approved-shaped record with an invalid GPL notice SHA. All three are rejected **before any network fetch**.

**After an authorized permanent public Builder Release exists**, independently fetch and check every public asset and tag/source commit. *Only then* may the owner-approved candidate values be copied into the live app lock with `kind: github-release-st-mt` and `status: release-approved`, followed by ST/MT rebuild, file/HTTP hosted QA and final release acceptance. A generated proposal by itself never authorizes publication or a lock change.

## 2026-10-17 — importer positive and negative proof before publication

Reverse Clip Draft PR #2 now exercises the **real PowerShell Release importer**
against both positive **local mocked GitHub Release files** and negative
staging/invalid metadata. Windows [CI #39008453901](https://github.com/ttomohisa/htmlapps-reverse-clip/actions/runs/39008453901)
passed its importer build job:

- The current exact ST and MT JS/WASM/loader cache was copied into local
  **synthetic transport fixture ZIPs**. The PowerShell `Invoke-WebRequest`
  function was intercepted to copy only explicitly permitted local files.
  No real public GitHub Release was queried or created.
- Both variants accepted correctly pinned ZIP hashes, a synthetic
  corresponding-source payload and a separately pinned GPL notice. The
  generated per-variant `verified-release-archive.json` markers matched
  source, notice, tag and binary archive SHA-256 values.
- A subsequent ST import passed using a verified **offline cached** release,
  with network calls explicitly forbidden by the test wrapper.
- The same CI also confirmed the importer rejects unrecognized draft-only
  lock kind, nonapproved release status and invalid GPL notice SHA **before
  any network call**.

The synthetic source and notice used in this transport test **are not valid
GPL publication artifacts**; this test proves the importer plumbing and
hash/caching mechanics, not source completeness, production access or
license compliance. The actual source-complete Builder package and
independent hash audit remain separate evidence.

**What the owner needs to decide at the later release gate, not now:**

1. Accept or revise the proposed distinct `video-reverse-v1.10.6`
   tag/source-commit and its build-info naming disclosure.
2. Review and approve the GPL/x264 corresponding-source archive,
   supplemental notice, and source availability plan (seek a qualified
   license review if needed).
3. Explicitly authorize the creation/publication of that profile Release
   after checking its manifest; then review the final app QA before manually
   merging PRs.

Until those decisions are made, the staging proposal stays non-activatable.
**There is no action required from the owner for ongoing Draft-PR tests.**

## 2026-10-17 — review-archive expiry resilience (verified)

The original source-complete Actions artifact **11920042823** expires on
**2026-10-21T18:41:00Z**. Earlier review workflow revisions depended on
that expiring ZIP for every fresh audit stage. This is now corrected without
changing the compiled binaries or weakening SHA checks:

- New offline verifier:
  [`scripts/verify-video-reverse-staged-promotion.py`](../scripts/verify-video-reverse-staged-promotion.py).
  It independently pins the **whole** ST and MT ZIPs and corresponding
  source archive to their original audited SHA-256, rechecks inner FFmpeg
  JS/WASM/loader digests, per-variant BUILDINFO, source tree members, the
  independently pinned GPL/x264 disclosure and the non-activatable app
  Release-lock proposal. It rejects missing files, altered bytes, modified
  approval flags and stale or inconsistent lock metadata.
- [Restaging CI #39010016614](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39010016614)
  **SUCCESS**: used preserved review artifact **12091513054** instead of the
  original expiring review ZIP. Byte validation passed, and review-only
  artifact **12093015125** was preserved through
  **2026-11-16T06:28:25Z**.
- [Latest rerun #39010150306](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39010150306)
  **SUCCESS**: automatically selected the latest unexpired matching review
  artifact (ID **12093015125**) and only then re-validated it against the
  same hard-pinned original JS/WASM/ZIP/source/addendum checksums. New
  review-only artifact **12093045270**, expires
  **2026-11-16T06:30:40Z**.
- Source selection is not a trust decision: the newest candidate is
  **rejected if its bytes differ** from original independent checksums.
  Re-staging only copies the verified files and refreshes a
  `REVIEW_RESTAGING_PROVENANCE.json` pointer; it never modifies or
  republishes compiled binaries, corresponding source, license addendum,
  `SHA256SUMS.txt` or the proposed app release lock.
- The lightweight workflow still only runs on relevant Builder Draft
  branch changes or manual dispatch. No periodic background job or public
  GitHub Release has been created. GitHub Actions artifacts **remain
  temporary and require another run while an unexpired candidate exists**;
  they do not satisfy permanent public GPL source availability.

**Release gate remains unchanged:** corresponding-source/legal review,
public immutable binary+source+notice distribution, unauthenticated public
URL/hash validation, approved app-lock promotion and final acceptance are
still outstanding. Never interpret a refreshed Actions artifact as a
published Release or authorisation to merge.

## 2026-10-17 — final review-package input hardening

[`scripts/verify-video-reverse-staged-promotion.py`](../scripts/verify-video-reverse-staged-promotion.py)
also rejects **unexpected file names, directories or symbolic links**
inside the review input. This is important for future publishing scripts:
no unaudited executable or substitute payload may ride along via a
wildcard file upload. All original ST/MT, source and notice SHA-256 pins
remain enforced.

[Final verification CI #39010376409](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/39010376409)
**SUCCESS**, using the existing reviewed candidate. Latest review-only
artifact **12092676288** (expires **2026-11-16T06:34:08Z**). No binary
recompilation, public tag, GitHub Release or app runtime modification.

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

## 2026-10-19 — unauthenticated public Release audit gate prepared (not published)

New read-only script: [public Release verifier](../scripts/verify-video-reverse-public-release.py). It is deliberately separate from staging; **never** uses an Actions token, creates a Release or tag, or activates the app lock.

- Queries the public Release by fixed proposed tag `video-reverse-v1.10.6`, rejecting draft/prerelease, absent or unexpected assets and unexpected direct-download URLs.
- Resolves lightweight or annotated Git tags, requiring exact compiled source commit `8e60da7cf9de644ce3f6cce9c007b17e6a385c53`.
- Requires **exactly seven** assets: two ST/MT binary ZIPs, corresponding source TAR, two BUILDINFO files, original SHA256SUMS and GPL/x264 notice addendum; downloads without authentication.
- Pins the three original whole ST/MT/source digests and addendum independently of the Release, then checks the five original checksums, internal FFmpeg JS/WASM/loader SHA-256, ZIP notices and separate BUILDINFO bytes.
- Optionally emits local evidence JSON with public asset links, SHA-256, byte counts, release tag and source commit; explicitly marks legal approval, reproduced build and consumer promotion as false.

Run the **offline rejection tests**, without network, rebuilt WASM or large binary downloads:

    python3 -m unittest discover -s tests -p 'test_video_reverse_public_release.py' -v

After separately authorized publication **only**, verify real anonymous downloads:

    python3 scripts/verify-video-reverse-public-release.py --evidence-json video-reverse-public-release-proof.json

The script fails closed until the public Release exists, and reports when GitHub does not attest immutable publication. It cannot establish legal corresponding-source completeness, independent source-to-binary reproduction or durable retention. Those, plus owner publication decision, separate final app ST/MT QA and G3 acceptance, remain P0 gates. No Release/tag/merge/app repin is authorized by these audit additions.

For the remaining corresponding-source/licensing and build-provenance decisions, use the [separate review worksheet](VIDEO_REVERSE_GPL_REVIEW_WORKSHEET.md). It records verified byte/source-presence evidence and intentionally leaves reviewer and owner sign-offs blank. Do not interpret the worksheet as legal clearance.
