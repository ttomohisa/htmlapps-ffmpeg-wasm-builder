# Video Reverse ST/MT — corresponding-source release review (not published)

**Date:** 2026-10-14 (JST)  
**Status:** Review-only packaging path staged. **No GPL corresponding-source archive, permanent public release, version tag or legal sign-off has been produced.** Do not ship the temporary binary as if it had permanent matching sources.

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

**Not executed yet:** the manual packaging workflow may only be invokable once GitHub makes this workflow available from the default branch. It currently exists only in an unmerged Draft branch; do **not** claim the source archive has been created. Review the exact workflow permissions and Actions quota before running. If the pinned artifact expires on October 20, use the full rebuild fallback on the exact source commit (more expensive), not a mutable or unverified substitute.

## Before any immutable public release

- Pass real ST/MT browser tests, phone + source-format QA and Reverse Clip G2 as required; separately review the source/archive structure and license obligations. This checklist is not a legal opinion.
- Use an appropriate **new immutable semantic version and release tag**, not an existing tagged version or in-place asset replacement, even though the experimental CI binary internally reports Builder 1.10.4.
- Confirm `ffmpeg-wasm-video-reverse-single-thread-vX.Y.Z.zip` and `ffmpeg-wasm-video-reverse-multi-thread-vX.Y.Z.zip`, corresponding complete sources, `SHA256SUMS.txt`, `BUILDINFO` and GPL/third-party license notices are all uploaded and publicly retrievable.
- The release source archive must reproduce the binary's FFmpeg, x264, Emscripten and exact Builder recipe, with their precise revisions; record license and source availability for the duration required by applicable obligations.
- Only then change Reverse Clip `reverse-runtime.lock.json` to the reviewed immutable Release importer with ZIP SHA-256 + ST/MT asset SHA-256. Rebuild both standalone HTML artifacts, test `file://` ST, isolated HTTPS MT, CSP and no user-media uploads. Preserve the app's silent default.
- **Owner alone** merges Draft PRs and authorizes production release. Browser Kitty integration remains deferred until the app is complete.

**Current decision:** Corresponding-source preparation is designed and byte-provenance pinned, but the durable distribution and source audit are **OPEN**; no publication, merge or tagging occurred.
