# Video Audio Extractor profile

Phase 4 profile for Browser Kitty's Video Audio Extractor.

The runner has three operations:

- inspect a media file and return the container/video-count/audio-stream fields needed by the app;
- copy exactly one selected audio stream into an approved audio container without decoding or re-encoding;
- transcode exactly one selected audio stream to M4A/AAC, WAV/PCM16, or MP3/LAME.

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
| MP3 | LAME 4.0 CBR at 128 / 192 / 256 / 320 kbps; mono or stereo |

Phase 4 enables decoders for AAC, ALAC, MP3, Opus, Vorbis and FLAC, then uses libswresample to adapt decoded audio to the selected encoder while preserving the selected track only. MP3 output uses the pinned LAME 4.0 source release; its frontend and decoder are disabled and only the static encoder library is linked.

The real-browser smoke test keeps the Phase 2/3 cases and additionally verifies LAME MP3 output at 128 kbps stereo and 320 kbps mono by re-inspecting the generated files. MP3 output is limited to mono/stereo; multichannel input can be downmixed to stereo.

No video decoder/encoder, libavfilter, libswscale, x264, or GPL-only component is enabled.
