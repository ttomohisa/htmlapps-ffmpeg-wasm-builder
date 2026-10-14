# Video Reverse ST/MT — corresponding-source release review (not published)

**Date:** 2026-10-14 (JST)  
**Status:** A review-only ST/MT binary + corresponding-source **candidate archive has been successfully created and independently integrity-checked**. **No permanent public GitHub Release, version tag, legal/GPL compliance sign-off or Android Gate G2 approval has been produced.** Do not ship the temporary candidate as a production version.

## New 1.10.6 experimental pthread cleanup (NOT released)

During exact 37.806s/39-frame H.264 High ST/MT testing with the 1.10.5
timeline fix, the 2-second tests passed, and ST could render the full
10-second reverse and ping-pong. An MT high-resolution 10-second
attempt failed in chunk 3/5 with `WebAssembly.Memory(): could not
allocate memory` after earlier chunk jobs. This is an actual Chrome
resource failure and remains G2-blocking until repeat+isolation tests
pass. It is not proof of Android OOM or a permanently leaked buffer.

A targeted v1.10.6 candidate adds explicit Emscripten nested pthread
pool teardown before returning each MT segment's MP4 bytes. Emscripten
`-sMODULARIZE=1` supports `--post-js` code inside its factory;
the video-reverse MT profile exports a scoped shutdown hook and the
browser runtime invokes it on success and failure. The ST path is
left unchanged. This is a **hypothesis-directed cleanup candidate**:
do not claim it fixed actual device memory behavior without rebuild,
actual browser and physical-device validation.

The 1.10.4 **review-only corresponding-source ZIP** is historical and
does not cover v1.10.5 or v1.10.6. Fresh matching source archives and
a reviewed immutable release remain mandatory before stable publication.

## New 1.10.5 experimental bugfix (NOT covered by the 1.10.4 review bundle)

On 2026-10-14 the owner reported an H.264 High source at 1920x1080,
37.806089 seconds, about **39 video samples** (avg 877500/850637 fps,
r_frame_rate 13/4, timebase 1/90000). ST succeeded while MT failed
with `Reverse frame count mismatch: expected=30 actual=24`.

Inspection of the public Presentation Video Maker source showed its fast
WebCodecs export stores **explicitly variable sample durations** (static
slide holds) in MP4. A synthetic source with matching sample count,
codec, average fps, duration, and 90k timestamp scale provoked a
related real ST WASM regression: a Ping-pong return chunk beginning at
0.067s expected 28 frames and produced only 9 (App CI #38527422037).
This proves the pre-FPS subclip trim does not reliably preserve long
display spans; it does **not by itself prove the user's MT 30->24
case is fully fixed**.

The experimental Builder version 1.10.5 branch now changes the ordering
to **source-timeline FPS normalization, then exact frame-index window,
then bounded Reverse**, without weakening frame-count validation. It
retains the 2s/120-frame reverse buffer guard and decodes source only
through the interval's end. Native FFmpeg filter trials for 0–2s,
0.067–1.933s and 6–8s produced correct 30/28/30 frames from that
fixture. **Fresh ST/MT WASM compilation, real browser regressions,
matching immutable source/binary release and actual Android acceptance
remain outstanding until independently verified.**

The previously generated **1.10.4 review artifact 11862943520**
is a source-complete candidate for the old 1.10.4 binaries only; it
must **not** be used as corresponding source for a new 1.10.5 binary.
The preparation workflow in this branch intentionally remains locked
to the historical 1.10.4 build and cannot attest to 1.10.5.

## Source / binary evidence verified

- The selected exact compiled source commit is `d1feee021aae6feb53eab1084664123f602c2085` (Builder `BUILDER_VERSION=1.10.4`).
- Proven `video-reverse` Chrome ST and isolated MT CI: [Builder run #38357268611](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/38357268611), temporary artifact ID **`11794061552`**, expires **2026-10-20 06:40:48 UTC**. The artifact ZIP contains both variants' JS, WASM, gzip files, manifests, smoke HTML only; **it does not contain the matching source archive or license bundle**.
- Reverse Clip's app lock independently pins the four byte hashes. The downloaded Builder Actions artifact was independently inspected (SHA-256 checks of all four files passed; the ST manifest reports no cross-origin isolation, while MT requires COOP/COEP and SharedArrayBuffer).

| Asset | ST SHA-256 | MT SHA-256 |
| --- | --- | --- |
| `ffmpeg.js` | `4a85934acca9afb09ec90be83958002040d100674fee7c7903e5a59ed1d8a74c` | `e88f95f77ee94acc8df3cef559e48e76cbf78a6175e831d712a471a48d614007` |
| `ffmpeg.wasm` | `4b8d462196b50dd7bbe51e1b329df3d53afa01022ebf5eea2ca23a65c3386b51` | `42ee5317ca6ce2cc139c481bb8de8182f9e1e4315c855718cf307f9d8afb2958` |

The common MIT wrapper `runtime/browser-ffmpeg.js` is pinned to `34c29442c74d9d178666d9788f5ba9f74bb1c56e2052dc0f94dc417ccb427d43`.

## Review-only packaging workflow

`.github/workflows/prepare-video-reverse-release.yml` performs **no GitHub Release, tag, merge or publication**. Its manually dispatched `reuse_verified_wasm` flag defaults to **true**:

1. Checkout the **exact Builder commit** above, not a moving `main` or Draft PR head.
2. While the pinned Actions artifact still exists, download the **original ST/MT WASM that already passed Chrome smoke**, avoiding another expensive Docker rebuild. Alternatively use `reuse_verified_wasm=false` to rebuild and smoke at the same pinned source.
3. Fail closed unless all four WASM/JS SHA-256 hashes, MIT wrapper hash, profile, version, threading and isolation metadata match the reviewed record.
4. Run existing `scripts/prepare-release.sh` with `RELEASE_PROFILE_SELECTION=video-reverse` to prepare ST and MT ZIPs, BUILDINFO, license notices, checksum list and the matching FFmpeg/x264/Emscripten/Builder source archive.
5. Validate required asset names, notices and SHA256SUMS then upload an **expiring review-only Actions artifact (7 days)**; inspect it before any release decision.

**Executed once, on the Draft branch:** [review-only Builder Actions #38494235345](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/38494235345) succeeded without any FFmpeg Docker rebuild. The special one-time branch-push trigger was removed afterward, so normal Draft PR updates no longer execute expensive corresponding-source packaging. The remaining `workflow_dispatch` path may require availability of this workflow on the default branch for manual reuse. Once original compiled artifact expires October 20, a new compile with exact source (and fresh hashes) is needed; never silently substitute binaries.

## 2026-10-14 actual candidate archive — integrity review PASS, distribution gate OPEN

- **Workflow:** [Builder run #38494235345](https://github.com/ttomohisa/htmlapps-ffmpeg-wasm-builder/actions/runs/38494235345), exact source checked out at `d1feee021aae6feb53eab1084664123f602c2085`.
- **Review-only artifact:** `11862943520`, name `video-reverse-source-complete-candidate-aca6c0718ec3bd6a21cd3cf17410c71cc84b8fc1`, **74,598,505 bytes**, expires **2026-10-21 01:10:58 UTC** (one week after generation).
- **Packaging result:** 2 binary ZIPs (ST and MT), matching source archive, both BUILDINFO files, SHA256SUMS (6 files total including the checksum list). Artifact is not a GitHub Release and has no immutable public distribution link.
- **Independent integrity checks outside GitHub Actions:** all five SHA256SUMS entries matched their archived bytes; the two binary ZIPs contained the exact four locked `ffmpeg.js`/`ffmpeg.wasm` hashes above and the common SHA-pinned `browser-ffmpeg.js`. Both ZIPs included FFmpeg/x264/Emscripten notices and licenses and their per-profile build metadata; both manifest threading/isolation modes matched expectation.
- **Source archive check:** `ffmpeg-wasm-sources-v1.10.4.tar.gz`, SHA-256 `0e957f10e93c7834e4ab39c39fc5ccab0efdd0462926c8c08fbeb8aac7a594fc`, contains **26,553 tar entries** including pinned FFmpeg `n9.0.1`, x264 `31e19f92f00c`, Emscripten `6.0.6`, and the full Builder 1.10.4 source/recipe (`build.sh`, `versions.env`, `profiles/video-reverse`, dedicated runner, common runtime). Additional source trees are present for libvpx, libwebp, Opus and LAME, inherited from the common Builder packaging path.
- Exact **binary ZIP SHA-256** (for future archival review, not approval to change the consumer lock): ST `e34eb78774e361e58eb76729b2eeab6949da39100532ebaba24ccf2208ad964d`; MT `6e24addde15476f061c43dcda1487fe4048d082812f051068c68834a1be75ed1`.
- **Still pending:** independent GPL/legal obligations review, public immutable release naming/version decision, confirmation that a public corresponding-source archive remains accessible for the required period, Android/G2 performance and audio quality, consumer re-lock and ST/MT release regression. This checklist/integrity check is **not** a legal opinion or production sign-off.

## Before any immutable public release

- Pass real ST/MT browser tests, phone + source-format QA and Reverse Clip G2 as required; separately review the source/archive structure and license obligations. This checklist is not a legal opinion.
- Use an appropriate **new immutable semantic version and release tag**, not an existing tagged version or in-place asset replacement, even though the experimental CI binary internally reports Builder 1.10.4.
- Confirm `ffmpeg-wasm-video-reverse-single-thread-vX.Y.Z.zip` and `ffmpeg-wasm-video-reverse-multi-thread-vX.Y.Z.zip`, corresponding complete sources, `SHA256SUMS.txt`, `BUILDINFO` and GPL/third-party license notices are all uploaded and publicly retrievable.
- The release source archive must reproduce the binary's FFmpeg, x264, Emscripten and exact Builder recipe, with their precise revisions; record license and source availability for the duration required by applicable obligations.
- Only then change Reverse Clip `reverse-runtime.lock.json` to the reviewed immutable Release importer with ZIP SHA-256 + ST/MT asset SHA-256. Rebuild both standalone HTML artifacts, test `file://` ST, isolated HTTPS MT, CSP and no user-media uploads. Preserve the app's silent default.
- **Owner alone** merges Draft PRs and authorizes production release. Browser Kitty integration remains deferred until the app is complete.

**Current decision:** Corresponding-source preparation is designed and byte-provenance pinned, but the durable distribution and source audit are **OPEN**; no publication, merge or tagging occurred.
