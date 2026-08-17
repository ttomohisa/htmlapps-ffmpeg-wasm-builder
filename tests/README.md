# Smoke-test fixture

`fixtures/smoke-input.mp4` is a tiny synthetic 1-second H.264 + AAC MP4 used only for automated runtime validation. It exercises the main video-compressor decode/filter/encode/mux path.

It was generated from synthetic FFmpeg sources (test pattern + sine wave), so it does not contain third-party media content.
