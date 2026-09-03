# video-compressor profile

- `ffmpeg.flags`: compact FFmpeg components required by the video-compressor runner.
- `../../runners/video-compressor.c`: public-libav decode/filter/encode/mux frontend.
- `single-html/template.html`: direct-file demo shell.

Outputs:

- H.264 (`libx264`) + AAC in MP4
- VP9 (`libvpx-vp9`) + Opus in WebM

The profile is intentionally single-threaded and does not expose arbitrary FFmpeg CLI arguments. Browser `File` / `Blob` inputs are mounted through Emscripten WORKERFS so large source videos are not copied wholesale into MEMFS before inspection or transcoding.

The runner also provides an inspection mode that measures average video bitrate from demuxed video packet bytes and stream duration, reports display-matrix rotation, and uses that display information for automatic 90/180/270-degree pixel rotation before scaling and encoding.

### VP9 speed modes

VP9 uses progressively deeper look-ahead for slower modes (0 / 8 / 16 / 25 frames) to trade browser memory and encoding time for compression efficiency. The default `fast` mode uses 8 frames.
