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


## Threading variants (Builder 1.10.3)

The profile builds `dist/video-compressor/single-thread/` and `dist/video-compressor/multi-thread/`. Both retain H.264/AAC MP4, VP9/Opus WebM, WORKERFS, autorotation and timing checks. ST remains suitable for direct `file://` use. MT requires HTTP(S), cross-origin isolation and SharedArrayBuffer; it is never substituted for ST automatically.

MT uses eight pooled pthread workers, two video decoder threads, four video encoder threads and one x264 lookahead thread. Audio encoding and filter graphs stay single-threaded. VP9 uses row multithreading. Actual speed depends on input and device, and small videos may not become faster.

Release assets use explicit `-single-thread-` and `-multi-thread-` names. The legacy `ffmpeg-wasm-video-compressor-v{version}.zip` remains a byte-identical ST alias. `pack-single-html.bat video-compressor` continues to select ST.
