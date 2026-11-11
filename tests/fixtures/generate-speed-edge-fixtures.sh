#!/usr/bin/env bash
# Synthetic-only test fixture. Created in CI before Docker build; no user media.
set -euo pipefail
cd "$(dirname "$0")"
command -v ffmpeg >/dev/null || { echo "Native ffmpeg is needed to generate the synthetic speed fixture" >&2; exit 1; }
ffmpeg -hide_banner -nostdin -loglevel error -y \
  -f lavfi -i 'testsrc2=size=96x64:rate=12:duration=2' \
  -f lavfi -i 'sine=frequency=1000:sample_rate=48000:duration=2' \
  -filter_complex '[0:v]setpts=PTS+7/TB,setsar=4/3[v];[1:a]asetpts=PTS+7.4/TB[a]' \
  -map '[v]' -map '[a]' -c:v libx264 -preset ultrafast -crf 35 \
  -pix_fmt yuv420p -bf 0 -c:a aac -b:a 48k \
  -copyts -start_at_zero -video_track_timescale 12000 speed-offset-sar.mp4
test -s speed-offset-sar.mp4
ffprobe -hide_banner -v error -show_entries format=start_time:stream=codec_type,start_time,sample_aspect_ratio -of compact speed-offset-sar.mp4
