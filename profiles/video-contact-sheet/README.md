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
--thumb-size 320                        # 96..960
--columns 4                             # optional
--partition-count 2                     # optional: 1 (default) or 2
--partition-index 0                     # optional: 0 (default) or 1, less than partition count
```

The JSON metadata contains duration, codec, detected quarter-turn rotation, grid/cell dimensions, and target/actual sample timestamps.

## Sampling behavior

Targets are evenly distributed from the beginning to the end of the video. For inter-frame codecs, the runner seeks backward to a keyframe and decodes forward until it reaches the target timestamp. This avoids decoding the entire movie just to obtain a small number of thumbnails.

The final end target is nudged slightly before EOF to avoid requesting a timestamp beyond the last decodable frame.

### Independent sample partitions

Two ordinary single-thread Worker instances can divide extraction without pthreads,
SharedArrayBuffer, or cross-origin isolation. Both receive the original `--count`
and geometry. Partition `p` extracts global sample indexes where `index % 2 == p`;
it does not split the duration or recalculate target times using a smaller count.
Each returned PPM keeps the complete sheet dimensions, with unowned cells black.
Its JSON contains only owned samples, retaining global indexes and target/actual
times, plus `partitionCount` and `partitionIndex`. A caller must validate both
partitions and merge their owned cell rectangles before presenting a complete sheet.

Omitting partition options preserves the existing output and metadata contract.
The manifest advertises `partitionSampling: true` and `maxPartitionCount: 2`.
Progress is local to the selected partition and reaches 1 after its own samples.
Each Worker has its own decoder, Wasm heap and output buffer: cap concurrent
instances at two, keep WORKERFS, and use a one-Worker fallback on constrained devices.
Partition support does not establish any particular speedup; measure on the target device.

## Browser/memory behavior

Input uses Emscripten WORKERFS, so a large browser `File`/`Blob` is read on demand instead of being copied wholesale into MEMFS. The output PPM is held in MEMFS and copied back to the app. With the default 320 px thumbnail long edge, even 48 thumbnails remain practical for a single-HTML app.

Runner 1.6.0 accepts integer thumbnail long edges from 96 through 960 px; the
default remains 320 px. The manifest advertises `minThumbSize: 96`,
`maxThumbSize: 960`, and `maxSheetPixels: 25000000`. The complete PPM grid,
including empty last-row slots and unowned partition cells, is rejected above
25,000,000 pixels before the output buffer is allocated. This retains every
previously valid grid at 640 px or below. Existing allocation-overflow and
allocation-failure checks remain in place.

For a PC-oriented high-resolution preset, consumers can use 960/840/660 px for
12/24/48 samples respectively. A 24-sample, 16:9 source with the default six
columns produces 5040×1892 px (840×473 px cells, rounded to whole pixels).
The largest square preset is 48×660² = 20,908,800 pixels. Use one Worker for these
large outputs: the limit applies per extraction, not to aggregate browser
memory. A 25-million-pixel RGB buffer alone is 75 MB, before the MEMFS output,
transfer copies, decoder, Wasm heap, Canvas and PNG/JPEG export buffers.
This is a bounded option for capable devices, not a guarantee that it fits all
browsers or phones. Defaults, sampling and the single-thread/file:// contract
are unchanged; no pthread variant is added.

## Licensing

No x264 or GPL-only FFmpeg component is enabled. The generated core is treated as `LGPL-2.1-or-later` under this Builder's release policy.
