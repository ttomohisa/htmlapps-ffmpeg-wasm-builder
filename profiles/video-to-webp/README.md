# video-to-webp

Specialized FFmpeg 9 WASM profile for short video clips -> animated WebP.

- common video decode/demux only; no audio pipeline
- trim, autorotate, fps reduction and scaling
- FFmpeg `libwebp_anim` encoder backed by pinned libwebp 1.6.0
- quality, lossless and compression-level controls
- infinite-loop animated WebP output
- WORKERFS input so large source videos are not copied wholesale into MEMFS
