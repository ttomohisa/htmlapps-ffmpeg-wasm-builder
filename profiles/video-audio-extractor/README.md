# Video Audio Extractor profile

Phase 1 profile for Browser Kitty's Video Audio Extractor.

The runner has two operations:

- inspect a media file and return only the container/video-count/audio-stream fields needed by the app;
- copy exactly one selected audio stream into an approved audio container without decoding or re-encoding.

Input `File` / `Blob` data is mounted with WORKERFS, so the browser does not copy the complete source video into MEMFS before processing. Output is currently written to MEMFS.

## Stream-copy matrix

| source codec | output |
|---|---|
| AAC | M4A |
| Opus | OPUS |

This Phase 1 matrix intentionally includes only combinations covered by the real-browser smoke test. ALAC, MP3, Vorbis, FLAC, PCM, AC-3 and E-AC-3 are deferred until their compatibility tests are added in the later compatibility phase.

The runner rejects codecs outside this matrix instead of guessing a container. MPEG-TS/raw ADTS AAC -> M4A uses FFmpeg's `aac_adtstoasc` bitstream filter.

No decoder, encoder, libavfilter, libswscale, libswresample, x264, or GPL-only component is enabled in this Phase 1 profile.
