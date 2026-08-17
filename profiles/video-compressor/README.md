# video-compressor profile

- `ffmpeg.flags`: minimal FFmpeg components required by the video-compressor runner.
- `../../runners/video-compressor.c`: public-libav decode/filter/encode/mux frontend.
- `single-html/template.html`: direct-file demo shell.

The default output is H.264 (libx264) + AAC in MP4. The profile is intentionally single-threaded and does not expose arbitrary FFmpeg CLI arguments.
