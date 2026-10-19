# video-reverse v1.10.6 — supplemental GPL and corresponding-source notice

**Release-review document (2026-10-17). NOT a published Release or legal sign-off.** This note supplements, but **does not modify**, the exact previously audited `THIRD_PARTY_NOTICES.md` embedded inside both `ffmpeg-wasm-video-reverse-*-v1.10.6.zip` binary ZIPs.

## Why this addendum is necessary

Those binary ZIPs carry `BUILDINFO.txt` and `manifest.json` specifying
`Profile: video-reverse`, `Generated core license: GPL-2.0-or-later`
and x264 linkage. They also carry `LICENSES/FFmpeg-COPYING.GPLv2`,
`LICENSES/x264-COPYING`, `LICENSES/FFmpeg-LICENSE.md`,
`LICENSES/Emscripten-LICENSE` and further third-party notices.

However, the **existing embedded** general `THIRD_PARTY_NOTICES.md`
enumerates other GPL/x264-enabled Builder profiles and **does not yet
mention `video-reverse` by name**. Its general assertion about source
being on "the same tagged release" also requires qualification here:
the embedded `BUILDINFO.txt` says `Release tag: v1.10.6`, while the
already-public Builder-wide `v1.10.6` Release **does not contain**
`video-reverse` ZIPs. Do not rely on that existing tag to obtain the
corresponding source for this particular profile.

This supplemental notice makes those details explicit without changing
the reviewed binary ZIPs, which would invalidate the original SHA-256
pins. Future newly built versions should correct the notice *inside*
the newly generated ZIP, rather than perpetuating this addendum.

## Actual license classification and included parts

- **video-reverse ST and MT:** generated FFmpeg WebAssembly cores linking
  **x264**, distributed under **GPL-2.0-or-later**. The MT variant also
  uses Emscripten pthread workers; that does not change the core's GPL
  classification.
- **FFmpeg:** the pinned `n9.0.1` source and applicable license text are
  included in the corresponding-source archive and each compiled
  binary ZIP, respectively.
- **x264:** the pinned `31e19f92f00c...` source and `COPYING` text
  accompany the binary/source review set. x264 is linked, not an
  optional unbundled component.
- **Emscripten/musl/compiler-rt:** their original upstream notice
  files accompany the binary ZIP; the pinned Emscripten 6.0.6 source
  tree is included in the reviewed source archive.
- **Builder-authored glue:** source in the Builder tree is subject to
  its own applicable MIT license; this does **not** relicense the
  generated FFmpeg/x264 WebAssembly core.
- **Consumer application:** Reverse Clip's original application source
  has its own MIT license, separate from its embedded GPL-licensed WASM.

The dedicated Builder profile produces bounded MP4 video chunks and
also exposes audio processing for the consuming app. The app is
responsible for its higher-level reverse/ping-pong and audio muxing;
`BUILDINFO` describing the video-chunk output does not mean the
consumer lacks audio support.

## Exact reviewed binary and source distribution proposal

- Builder source commit for the **existing audited compiled bytes**:
  `8e60da7cf9de644ce3f6cce9c007b17e6a385c53`.
- Builder version recorded in the binary: **1.10.6**.
- Existing Builder-wide `v1.10.6` Release: **not suitable** as the
  publication target for this profile.
- Separately proposed profile-scoped GitHub Release tag:
  **`video-reverse-v1.10.6`** (not created; owner approval required).
  The above `BUILDINFO` metadata must be explained in that Release's
  public notes; it must **not** be silently altered while claiming the
  original checked binary SHA-256.
- Binary ZIPs, matching `ffmpeg-wasm-sources-v1.10.6.tar.gz`, both
  `BUILDINFO` files, `SHA256SUMS.txt`, and this supplemental notice
  should all be available together **if and when** the owner authorizes
  a durable public release. The corresponding-source archive includes
  FFmpeg, x264, Emscripten and Builder build recipes.

**Publishing requirements still open:** confirm applicability of GPL
obligations and the legal completeness of corresponding source
(including any required patches/build or installation information),
version/tag/source-to-binary provenance, public availability and
integrity for the required period, and Reverse Clip's separate final
release gate. Checksums and source-tree presence alone do not establish
legal compliance or a reproduced build. This is an engineering audit
and license disclosure supplement, **not legal advice**.

See [the exact binary/source publication handoff](VIDEO_REVERSE_PUBLICATION_HANDOFF.md)
and [independent source audit](VIDEO_REVERSE_SOURCE_RELEASE.md).
