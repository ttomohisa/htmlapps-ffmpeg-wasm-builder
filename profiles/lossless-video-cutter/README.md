# Lossless Video Cutter profile

A compact public-libav runner that cuts media without decoding or re-encoding.
It copies compressed video/audio packets into a new container and therefore
keeps the original codec quality.

## Browser-facing contract

The runner accepts:

```text
--input /workerfs/input.mp4
--output /output.mp4
--start 13.4
--end 102.8
--no-audio        # optional
```

`--start` and `--end` are seconds on the input timeline. The output extension
selects the muxer, so the app can keep the input container (`.mp4`, `.mov`, `.mkv`, `.webm`) when that container is compatible with the
copied codecs.

Because compressed inter-frame video can only begin cleanly on a keyframe,
`--start` is **keyframe aligned**. The runner seeks backward and starts from the
nearest decodable keyframe at or before the requested time. It logs both the
requested and actual start time so the UI can explain any adjustment.

The runner copies the best video stream and, by default, the best audio stream.
It does not decode, encode, resize, filter, or change bitrate/fps.

## Large input files

This profile exports Emscripten WORKERFS. In browser integrations pass the selected `File`/`Blob` with `workerfs: true` under a path such as `/workerfs/input.mp4`. WORKERFS is read-only and slice-backed, so the whole input does not need to be copied to MEMFS before FFmpeg starts. The output is still created in MEMFS in the current runtime.
