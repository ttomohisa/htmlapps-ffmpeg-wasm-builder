#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROFILE="${1:-video-compressor}"

# shellcheck disable=SC1091
source "$ROOT/versions.env"
[[ -f "$ROOT/profiles/$PROFILE/ffmpeg.flags" ]] || { echo "Missing profile: $PROFILE/ffmpeg.flags" >&2; exit 1; }
[[ -f "$ROOT/profiles/$PROFILE/profile.env" ]] || { echo "Missing profile metadata: $PROFILE/profile.env" >&2; exit 1; }
[[ -f "$ROOT/runners/$PROFILE.c" ]] || { echo "Missing runner: runners/$PROFILE.c" >&2; exit 1; }

PROFILE_USE_X264="$(grep '^PROFILE_USE_X264=' "$ROOT/profiles/$PROFILE/profile.env" | tail -n 1 | cut -d= -f2- | tr -d '\r[:space:]')"
case "$PROFILE_USE_X264" in
  1) EXPORT_TARGET="export-with-x264" ;;
  0) EXPORT_TARGET="export-no-x264" ;;
  *) echo "Invalid PROFILE_USE_X264 in profiles/$PROFILE/profile.env" >&2; exit 1 ;;
esac

OUT_DIR="$ROOT/dist/$PROFILE"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

cache_args=()
if [[ "${GITHUB_ACTIONS:-}" == "true" ]]; then
  cache_args=(--cache-from "type=gha,scope=$PROFILE" --cache-to "type=gha,mode=max,scope=$PROFILE")
  echo "[FFmpeg WASM] Builder: GitHub Actions Buildx builder / target=$EXPORT_TARGET"
else
  docker buildx version >/dev/null
  echo "[FFmpeg WASM] Builder: selected Buildx builder / target=$EXPORT_TARGET (no forced builder/context)"
fi

docker buildx build \
  "${cache_args[@]}" \
  --file "$ROOT/docker/Dockerfile" \
  --target "$EXPORT_TARGET" \
  --build-arg "BUILDER_VERSION=$BUILDER_VERSION" \
  --build-arg "EMSDK_VERSION=$EMSDK_VERSION" \
  --build-arg "EMSCRIPTEN_COMMIT=$EMSCRIPTEN_COMMIT" \
  --build-arg "FFMPEG_REPOSITORY=$FFMPEG_REPOSITORY" \
  --build-arg "FFMPEG_REF=$FFMPEG_REF" \
  --build-arg "FFMPEG_COMMIT=$FFMPEG_COMMIT" \
  --build-arg "X264_REPOSITORY=$X264_REPOSITORY" \
  --build-arg "X264_FALLBACK_REPOSITORY=$X264_FALLBACK_REPOSITORY" \
  --build-arg "X264_REF=$X264_REF" \
  --build-arg "X264_COMMIT=$X264_COMMIT" \
  --build-arg "PROFILE=$PROFILE" \
  --output "type=local,dest=$OUT_DIR" \
  "$ROOT"

for file in ffmpeg.js ffmpeg.wasm ffmpeg.js.gz ffmpeg.wasm.gz manifest.json smoke-test.html; do
  [[ -s "$OUT_DIR/$file" ]] || { echo "Missing build output: $file" >&2; exit 1; }
done

printf '\n[OK] Build output: %s\n' "$OUT_DIR"
find "$OUT_DIR" -maxdepth 1 -type f -printf '%f %k KB\n' | sort

"$ROOT/scripts/smoke-test.sh" "$PROFILE"
