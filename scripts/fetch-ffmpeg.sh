#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_ffmpeg_env
mkdir -p "$SRC_DIR"
print_toolchain "source"
log "Fetching FFmpeg"
clone_exact_commit "$FFMPEG_REPOSITORY" "" "$FFMPEG_COMMIT" "$SRC_DIR/ffmpeg"
