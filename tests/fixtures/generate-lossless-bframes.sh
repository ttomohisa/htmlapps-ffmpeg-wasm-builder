#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
output="tests/fixtures/lossless-bframes.mp4"
command -v ffmpeg >/dev/null || { echo "Native ffmpeg with libx264 is required for the lossless cutter regression fixture" >&2; exit 1; }
command -v ffprobe >/dev/null || { echo "Native ffprobe is required for fixture validation" >&2; exit 1; }
ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i 'testsrc2=size=128x72:rate=25:duration=3' \
  -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=3' \
  -f lavfi -i 'sine=frequency=660:sample_rate=48000:duration=3' \
  -map 0:v:0 -map 1:a:0 -map 2:a:0 \
  -c:v libx264 -preset veryfast -crf 35 -pix_fmt yuv420p \
  -x264-params 'bframes=3:b-adapt=0:keyint=50:min-keyint=50:scenecut=0' \
  -c:a aac -b:a 40k -shortest -movflags +faststart "$output"
if ! ffprobe -v error -select_streams v:0 -show_entries frame=pict_type \
    -of csv=p=0 "$output" | grep -q 'B'; then
  echo "Synthetic cutter input must contain reordered B frames" >&2
  exit 1
fi
echo "Generated $output with H.264 B frames + two AAC audio tracks"
