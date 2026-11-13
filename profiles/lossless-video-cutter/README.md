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

The runner copies the best video stream, all compatible audio tracks (unless
`--no-audio` is selected), and compatible subtitle tracks. It logs omissions
for subtitles that cannot be represented in the output container; other stream
types, chapters, and extra video tracks are not preserved. It does not decode,
encode, resize, filter, or change bitrate/fps.

The requested end is a presentation-time boundary, not always the exact final
frame of the saved file. B-frame reorder can require a reference picture with
a later presentation timestamp. The runner retains such reference packets,
continues reading until the chosen video and audio decode timelines pass the
end, and may extend the visible tail slightly rather than corrupt the final
B frames. Test the saved file if exact frame boundaries matter.

## Large input files

This profile exports Emscripten WORKERFS. In browser integrations pass the selected `File`/`Blob` with `workerfs: true` under a path such as `/workerfs/input.mp4`. WORKERFS is read-only and slice-backed, so the whole input does not need to be copied to MEMFS before FFmpeg starts. The output is still created in MEMFS in the current runtime.

## Reproducible B-frame regression test

The real-browser smoke must run with a synthetic H.264 input containing
reordered B frames and AAC audio; the old one-second fixture contains no
reordered frames. The CI job generates `tests/fixtures/lossless-bframes.mp4`
using `tests/fixtures/generate-lossless-bframes.sh` and verifies that B frames
are present. For a local `./build.sh lossless-video-cutter`, install native
`ffmpeg` and `ffprobe` first. The script creates only synthetic media; it
does not fetch files or use user videos.
