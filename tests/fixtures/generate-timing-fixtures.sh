#!/usr/bin/env bash
# Optional regeneration. These synthetic fixtures have no private source media.
set -euo pipefail
cd "$(dirname "$0")"
ffmpeg -v error -f lavfi -i 'testsrc2=size=64x48:rate=24:duration=3' \
  -c:v libx264 -preset veryfast -crf 28 -bf 0 -an -y timing-cfr.mp4
ffmpeg -v error -f lavfi -i 'testsrc2=size=64x48:rate=25:duration=0.24' \
  -c:v libx264 -preset veryfast -crf 28 -bf 0 -an \
  -bsf:v "setts=ts='if(eq(N,0),0,if(eq(N,1),83,if(eq(N,2),207,if(eq(N,3),249,if(eq(N,4),491,532)))))':duration=41:time_base=1/1000" \
  -video_track_timescale 1000 -y timing-vfr.mp4
ffmpeg -v error -f lavfi -i 'testsrc2=size=64x48:rate=10:duration=0.1' \
  -c:v libx264 -preset veryfast -crf 28 -bf 0 -an -y timing-single.mp4
