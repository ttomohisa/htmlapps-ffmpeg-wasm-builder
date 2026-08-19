# Media Inspector profile

Small FFmpeg/libavformat inspection core for Browser Kitty's **Media Inspector**.

The runner opens a media file, asks libavformat to discover stream information,
and writes a structured JSON report. It does **not** decode or encode frames.

## Runner API

```text
--input /workerfs/input.mp4
--output /report.json
```

Browser runtime:

```js
const inputPath = "/workerfs/input.mp4";
const reportPath = "/report.json";

const result = await runner.run({
  files: [{ name: inputPath, data: file, workerfs: true }],
  outputs: [reportPath],
  args: BrowserFFmpeg.mediaInspectorArgs({
    input: inputPath,
    output: reportPath
  })
});

const report = BrowserFFmpeg.decodeJsonOutput(result, reportPath);
```

## Report contents

- container name, long name, file size, duration, start time, total bitrate
- all streams including video, audio, subtitle, data, and attachments
- codec name/long name, codec tag/FourCC, profile, level, bitrate
- video resolution, FPS rationals, pixel format, field order, SAR
- color range/primaries/transfer/space/chroma location
- HDR signals: PQ/HLG, mastering display, MaxCLL/MaxFALL, Dolby Vision,
  HDR10+, SMPTE 2094-50 presence
- rotation/display matrix
- audio sample rate, channels/layout, sample format, bitrate
  - if the container exposes only an unspecified 2-channel count, `channelLayout`
    uses FFmpeg's canonical `stereo` default for display, `channelLayoutReported`
    preserves the raw description, and `channelLayoutInferred` is `true`
- format/stream/chapter metadata
- chapters and stream dispositions

Common MP4/MOV/M4A, Matroska/WebM, AVI, MPEG-TS/PS, FLV, ASF,
Ogg, MP3, WAV, FLAC, AAC, AC-3/E-AC-3, and several raw video inputs are enabled.

## Browser compatibility / Media Doctor

The WASM core intentionally reports media facts rather than hard-coding browser
support policy. The app should combine `format.name`, `codec.name`, `codec.tag`,
profile/level, HDR data, and audio codec information with browser APIs such as
`HTMLMediaElement.canPlayType()` / `MediaCapabilities` to explain why the current
browser may not play a file.

That keeps browser-specific support decisions in the UI layer while the WASM
report remains deterministic and reusable.

## Size model

- no decoder
- no encoder
- no muxer
- no libavfilter
- no libswscale
- no libswresample
- no x264
- no pthread
- WORKERFS input, so a large File/Blob is not copied wholesale into MEMFS

The generated FFmpeg core remains LGPL-2.1-or-later because this profile enables
no GPL-only component and links no x264.
