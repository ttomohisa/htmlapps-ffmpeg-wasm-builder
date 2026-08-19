# Video Contact Sheet profile

`video-contact-sheet` is a small browser-oriented FFmpeg runner for generating a visual overview of a video.

It seeks across the full video duration, decodes one frame at each evenly distributed target, converts frames to RGB24 with `libswscale`, and assembles them into one binary PPM (P6) contact sheet. The intended app then draws the PPM to Canvas and exports PNG/JPEG with browser-native APIs.

## Why PPM?

The profile deliberately does **not** link a PNG/JPEG encoder or FFmpeg muxer. PPM is trivial for the runner to write and trivial for JavaScript to parse, so the Wasm stays focused on the expensive part that browsers cannot reliably provide for every source codec: seeking and decoding.

## Supported sample counts

- 12: default 4 x 3 grid
- 24: default 6 x 4 grid
- 48: default 8 x 6 grid

`--columns` can override the grid if an app needs a different layout. `--thumb-size` controls the long edge of each thumbnail. The default is 320 px.

## Supported video decoding

The profile enables common browser/file-source codecs including H.264, HEVC/H.265 (including Main/Main10 streams such as Pixel `hvc1` recordings), VP8, VP9, AV1, MPEG-4 Part 2, MPEG-1/2 Video, MJPEG, ProRes, and Theora. Common MP4/MOV, MKV/WebM, AVI, MPEG-TS/PS, FLV, ASF, and Ogg containers are enabled.

HEVC decoding here is performed by FFmpeg in Wasm; it does not depend on whether the browser itself can play HEVC.

## Runner API

```text
--input /workerfs/input.mp4
--output /contact-sheet.ppm
--metadata-output /contact-sheet.json   # optional
--count 12                              # 12 / 24 / 48
--thumb-size 320                        # 96..640
--columns 4                             # optional
```

The JSON metadata contains duration, codec, detected quarter-turn rotation, grid/cell dimensions, and target/actual sample timestamps.

## Sampling behavior

Targets are evenly distributed from the beginning to the end of the video. For inter-frame codecs, the runner seeks backward to a keyframe and decodes forward until it reaches the target timestamp. This avoids decoding the entire movie just to obtain a small number of thumbnails.

The final end target is nudged slightly before EOF to avoid requesting a timestamp beyond the last decodable frame.

## Browser/memory behavior

Input uses Emscripten WORKERFS, so a large browser `File`/`Blob` is read on demand instead of being copied wholesale into MEMFS. The output PPM is held in MEMFS and copied back to the app. With the default 320 px thumbnail long edge, even 48 thumbnails remain practical for a single-HTML app.

## Licensing

No x264 or GPL-only FFmpeg component is enabled. The generated core is treated as `LGPL-2.1-or-later` under this Builder's release policy.
