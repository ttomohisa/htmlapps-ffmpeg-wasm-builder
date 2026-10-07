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
