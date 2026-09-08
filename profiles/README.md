# Build profiles

Each profile contains `profile.env`, `ffmpeg.flags`, documentation, a public-libav runner, and a browser smoke test. Single-thread is the default. Profiles must opt in explicitly to additional variants with `PROFILE_THREADING_VARIANTS`.

Current release profiles are `video-compressor`, `video-speed-changer`, `lossless-video-cutter`, `media-inspector`, `video-contact-sheet`, `video-to-gif`, `video-to-webp`, and `ffmpeg-filter-builder`.

`ffmpeg-filter-builder` declares `PROFILE_THREADING_VARIANTS="single-thread,multi-thread"` and `PROFILE_PTHREAD_POOL_SIZE=8`, `PROFILE_DECODER_THREAD_COUNT=2`, `PROFILE_ENCODER_THREAD_COUNT=4`, and `PROFILE_X264_LOOKAHEAD_THREAD_COUNT=1`. It is intentionally staged: the runner accepts real video/audio filter chains, bounded time-range rendering, and H.264/AAC MP4 output, while full multi-input complex graph execution and drawtext/font dependencies are added only when their app milestones are implemented and smoke-tested.
