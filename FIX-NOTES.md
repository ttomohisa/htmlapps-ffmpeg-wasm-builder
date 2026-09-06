# Video Speed Changer build/smoke fix (Builder v1.7.2)

The v1.7.1 patch incorrectly added `buffer`, `buffersink`, `abuffer`, and `abuffersink` to `--enable-filter=` and to `CONFIG_*_FILTER` assertions. FFmpeg n9.0.1 registers these graph endpoints manually, so they are not configurable filter components. v1.7.2 removes those invalid flags/assertions.

The speed runner also keeps the v1.7.1 timestamp fix, but now preserves the input stream time base for the H.264 encoder when it is valid. The effective frame rate is still reported as source FPS multiplied by playback rate. Keeping a high-resolution input time base prevents fractional `setpts` output from collapsing onto duplicate encoder PTS values.

For an exact 1.0x conversion, the speed-specific `setpts` and `atempo` filters are skipped. This makes the baseline path match the already-proven Video Compressor filtering path as closely as possible.

Browser smoke-test diagnostics retain the playback-rate prefix and recent FFmpeg log tail so any remaining runtime failure identifies the exact rate and libav error.
