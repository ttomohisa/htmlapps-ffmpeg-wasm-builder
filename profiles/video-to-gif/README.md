# video-to-gif

Specialized FFmpeg 9 WASM profile for short video clips -> animated GIF.

- common video decode/demux only; no audio pipeline
- trim, autorotate, positioned crop, fps reduction and scaling
- two-pass `palettegen` -> `paletteuse` runner for GIF quality without retaining the whole clip in one filter graph
- adjustable color count and dithering
- infinite-loop GIF output
- WORKERFS input so large source videos are not copied wholesale into MEMFS
