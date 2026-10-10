# Video Compressor multi-thread design

Approved direction: retain a portable single-thread Compressor and add a dedicated multi-thread variant with H.264/AAC MP4 and VP9/Opus WebM. The Filter Builder runtime cannot replace the Compressor profile because it omits VP9/Opus output.

## Builder
Opt the Compressor profile into the existing dual-runtime build, manifest, embedded worker and smoke-test machinery. Keep ST free of pthreads and SharedArrayBuffer. Propagate the variant into x264, libvpx and Opus compilation. MT uses the reference pool budget of eight, two video decoder threads, four video encoder threads and one x264 lookahead thread. Audio remains single-threaded. Preserve all existing codecs, timing, autorotation and WORKERFS behavior. VP9 enables row-based multi-threading. Keep the legacy ST release asset as a compatibility alias alongside explicit ST/MT assets. Patch version is 1.10.3; publication requires a separate owner-approved release.

## Consuming application
The Compressor keeps index.html / video-compressor.html as ST and adds index.mt.html / video-compressor.mt.html, plus matching self-extracting variants. It embeds checksum-verified matching Builder assets, never runtime network dependencies. MT refuses unsupported non-isolated environments with actionable bilingual guidance; it does not silently execute ST. Preserve cancellation, disposal, both codec paths and recent dialog fixes. Replace only the drop-area and estimated-size gradients with solid existing theme colors. App patch version is 1.3.6.

## Distribution
Browser Kitty selects the Compressor MT source with a narrowly scoped COOP same-origin, COEP require-corp and CORP same-origin rule for /apps/video-compressor.html, following its Filter Builder pattern. Preserve portable ST download guidance. Correct obsolete H.265 claims only in the Compressor entry. Do not alter other apps, account permissions, secrets, or production directly.

## Validation and handoff
Run source tests and official Docker/browser CI for both Builder variants. Verify output codec, audio, dimensions, timestamps and actual threading; cover repeated runs, cancel/retry and ST regressions. Run Compressor source/build/wrapper/lifecycle/layout tests and actual hosted MT compression of synthetic media. Draft English PRs are dependency ordered: Builder, release, Compressor, then Browser Kitty. Owner performs merges; release/tag needs separate approval. A missing published runtime is a dependency blocker, never bypassed with an unverified asset.
