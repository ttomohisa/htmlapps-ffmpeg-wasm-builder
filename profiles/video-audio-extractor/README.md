# Video Audio Extractor profile

Phase 4 profile for Browser Kitty's Video Audio Extractor.

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
| MP3 | LAME 4.0 / libmp3lame at 128 / 192 / 256 / 320 kbps, mono or stereo |

Phase 4 keeps the AAC/WAV path and adds the pinned LAME 4.0 `libmp3lame` encoder. The real-browser smoke test generates MP3 at 128 / 192 / 256 / 320 kbps and re-inspects both stereo and mono output. Source audio with more than two channels is expected to be downmixed by the consuming app to stereo for MP3.

No video decoder/encoder, libavfilter, libswscale, x264, or GPL-only component is enabled. LAME is LGPL-licensed and is linked only into this profile.
