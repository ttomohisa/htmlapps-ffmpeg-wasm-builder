## v1.9.8 Filter Builder drawtext support

The Browser Kitty v0.7 text milestone needs `drawtext`, which FFmpeg 9 requires together with FreeType and HarfBuzz. The Filter Builder profile now opts into Emscripten's pinned `freetype` and `harfbuzz` ports and enables `drawtext`. The Builder runtime still contains no font asset; applications pass a local font file into the virtual filesystem and reference it with `fontfile=/...`, preserving fully-local runtime behavior and keeping font licensing/application choice outside the shared core.

The smoke test intentionally references a missing font and requires a font-loading error rather than `No such filter`; this proves the drawtext code path is linked without adding a font binary to Builder release assets.

## v1.9.7 Filter Builder speed PTS fix

Speed-up filters compress presentation timestamps. In v1.9.6 the Filter Builder H.264 encoder used `time_base = 1 / frame_rate`; for a 30 fps input, `setpts=PTS/1.5` produces frame spacing smaller than one 1/30-second encoder tick. Rescaling therefore collapsed adjacent frames onto identical PTS values, x264 reported `non-strictly-monotonic PTS`, and the MP4 muxer rejected duplicate DTS values.

v1.9.7 gives the video filter graph a clock of at least 90 kHz, rescales decoded frame PTS into that clock before `setpts`, and uses the same fine-grained clock for the video encoder. The graph can therefore represent accelerated timelines without integer timestamp collisions. A dedicated ST/MT smoke render now executes `setpts=PTS/1.5,scale=160:90` and inspects the resulting MP4 duration.

## v1.9.6 Filter Builder bounded-range/runtime geometry fix

A real 36-second H.264/AAC input exposed two gaps not covered by the v1.9.5 one-second smoke fixture. First, `trim` / `atrim` can finish their filter graph while demux still has packets inside the decode guard. `av_buffersrc_add_frame_flags()` then reports `AVERROR_EOF`; this is now accepted as normal completion so encoder flush and MP4 trailer writing still run.

Second, the v1.9.5 runner opened the encoder from the source geometry and appended a final scale to that size after the caller filter. A graph such as `scale=480:-2` was therefore scaled back to 1920x1080. v1.9.6 performs a configuration-only filtergraph probe first, reads the negotiated sink geometry, opens the encoder at that geometry, then rebuilds the real graph.

# Video Speed Changer build/smoke fix (Builder v1.7.2)

The v1.7.1 patch incorrectly added `buffer`, `buffersink`, `abuffer`, and `abuffersink` to `--enable-filter=` and to `CONFIG_*_FILTER` assertions. FFmpeg n9.0.1 registers these graph endpoints manually, so they are not configurable filter components. v1.7.2 removes those invalid flags/assertions.

The speed runner also keeps the v1.7.1 timestamp fix, but now preserves the input stream time base for the H.264 encoder when it is valid. The effective frame rate is still reported as source FPS multiplied by playback rate. Keeping a high-resolution input time base prevents fractional `setpts` output from collapsing onto duplicate encoder PTS values.

For an exact 1.0x conversion, the speed-specific `setpts` and `atempo` filters are skipped. This makes the baseline path match the already-proven Video Compressor filtering path as closely as possible.

Browser smoke-test diagnostics retain the playback-rate prefix and recent FFmpeg log tail so any remaining runtime failure identifies the exact rate and libav error.

## v1.9.5 candidate compile fix

The first v1.9.5 candidate accidentally removed the existing `PacketMeasure` typedef while refactoring the progress/range helpers. `--inspect-output` still referenced that type, so the public-libav runner failed at C compile time before linking. The typedef is restored near the other runner state structs, and `scripts/check-repository.ps1` now asserts that it remains present.
## CI repository-check fix

`check-repository.ps1` already registers `runtime/browser-ffmpeg.js` as `$runtime`. The v1.9.5 assertions accidentally referenced an undeclared `$browserRuntime` variable under `Set-StrictMode -Version Latest`, causing CI to stop before the actual literal checks. The assertions now use `$runtime` consistently.

