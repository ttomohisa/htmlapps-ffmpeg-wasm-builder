# Synthetic timing fixtures

Regenerate with `bash tests/fixtures/generate-timing-fixtures.sh` and a native FFmpeg
with libx264. Fixtures are 64×48, video-only, have no private source media, and use
no input B-frames so their intended presentation intervals are explicit.

- `timing-cfr.mp4`: 72 frames at 24 fps, exactly 3 seconds.
- `timing-vfr.mp4`: six frames at presentation times 0, 0.083, 0.207, 0.249,
  0.491, 0.532 seconds; durations 0.083, 0.124, 0.042, 0.242, 0.041, 0.041 seconds;
  exactly 0.573 seconds on a 1/1000-second grid. This catches replacing Original timing with a guessed
  constant frame-rate grid. The setts bitstream filter supplies explicit packet
  timestamps and the positive final packet duration.
- `timing-single.mp4`: one frame lasting 0.1 seconds at 10 fps.

The encoder output may reorder B-frames. Tests inspect `stts`, `ctts`, and edit
coverage rather than relying on average frame rate. GIF expectations round the
adjacent presentation timestamps to centiseconds, with an independently rounded
final packet duration, matching the format's quantization. A decoder may mask
short encoded GIF delays; these tests assert encoded timing, not visual playback.

## Filter Builder fixtures

- `timing-filter-cfr.mp4`: existing synthetic QA source, 320×180, 30 fps,
  180 H.264 frames, exactly 6 seconds, mono 48 kHz AAC carrying a 440 Hz tone.
  Video time base is 1/15360. No user media is included.
  SHA-256: `215090e91b31c4e95a2227de4876c8c8d6822b3c856e04110527d8531e9474a4`.
- `timing-filter-vfr-tail.mp4`: the same six PTS as `timing-vfr.mp4`, with
  final duration 0.073 seconds and endpoint 0.605 seconds. The final duration
  deliberately differs from the preceding 0.041-second gap, exposing a fallback
  that incorrectly copies the previous interval. Video time base is 1/1000.
  SHA-256: `897e09205a30ae5d80771609b58315766a49272a3574f146266fc48c28fa39cc`.
  Regenerate from the existing synthetic VFR source, never from test output:

  ```sh
  ffmpeg -i tests/fixtures/timing-vfr.mp4 -map 0:v -c copy \
    -bsf:v setts=duration=73:time_base=1/1000 -video_track_timescale 1000 \
    tests/fixtures/timing-filter-vfr-tail.mp4
  ```
