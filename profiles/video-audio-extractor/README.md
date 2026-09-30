# Video Audio Extractor profile

Phase 3 profile for Browser Kitty's Video Audio Extractor.

The runner has two operations:

- inspect a media file and return only the container/video-count/audio-stream fields needed by the app;
- copy exactly one selected audio stream into an approved audio container without decoding or re-encoding.

Input `File` / `Blob` data is mounted with WORKERFS, so the browser does not copy the complete source video into MEMFS before processing. Output is currently written to MEMFS.

## Stream-copy matrix

| source codec | output |
|---|---|
| AAC | M4A |
| ALAC | M4A |
| MP3 | MP3 |
| Opus | OPUS |
| Vorbis | OGG |
| FLAC | FLAC |

Every row in this stream-copy matrix is covered by the real-browser smoke test. AC-3 and E-AC-3 remain deferred until dedicated compatibility cases are added. The runner rejects codecs outside the tested matrix instead of guessing a container. MPEG-TS/raw ADTS AAC -> M4A uses FFmpeg's `aac_adtstoasc` bitstream filter.

## Transcode matrix

| output | encoding |
|---|---|
| M4A | FFmpeg native AAC at 128 / 192 / 256 kbps |
| WAV | PCM signed 16-bit little-endian |

Phase 3 enables decoders for AAC, ALAC, MP3, Opus, Vorbis and FLAC, then uses libswresample to adapt decoded audio to the selected encoder while preserving the selected track only. The real-browser smoke test covers Opus -> AAC/M4A at 192 kbps and AAC -> PCM16/WAV, then re-inspects both outputs.

No video decoder/encoder, libavfilter, libswscale, x264, external audio codec library, or GPL-only component is enabled.
