#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
: "${PROFILE:?}"
: "${OUT_DIR:?}"

MODE_OUT="$OUT_DIR"
TEMPLATE="/workspace/tests/smoke-test.template.html"
RUNTIME="/workspace/runtime/browser-ffmpeg.js"
FIXTURE="/workspace/tests/fixtures/smoke-input.mp4"
SMOKE_BODY="/workspace/tests/smoke-tests/${PROFILE}.js"
OUTPUT="$MODE_OUT/smoke-test.html"

for path in "$TEMPLATE" "$RUNTIME" "$FIXTURE" "$SMOKE_BODY" \
  "$MODE_OUT/ffmpeg.js.gz" "$MODE_OUT/ffmpeg.wasm.gz"; do
  [[ -s "$path" ]] || fail "Smoke-test packaging input is missing: $path"
done

base64_one_line() { base64 -w 0 "$1"; }

: > "$OUTPUT"
while IFS= read -r line || [[ -n "$line" ]]; do
  case "$line" in
    __FFMPEG_JS_GZIP_BASE64__)
      base64_one_line "$MODE_OUT/ffmpeg.js.gz" >> "$OUTPUT"
      printf '\n' >> "$OUTPUT"
      ;;
    __FFMPEG_WASM_GZIP_BASE64__)
      base64_one_line "$MODE_OUT/ffmpeg.wasm.gz" >> "$OUTPUT"
      printf '\n' >> "$OUTPUT"
      ;;
    __SMOKE_INPUT_BASE64__)
      base64_one_line "$FIXTURE" >> "$OUTPUT"
      printf '\n' >> "$OUTPUT"
      ;;
    __FFMPEG_RUNTIME__)
      cat "$RUNTIME" >> "$OUTPUT"
      printf '\n' >> "$OUTPUT"
      ;;
    __SMOKE_TEST_BODY__)
      sed 's/^/        /' "$SMOKE_BODY" >> "$OUTPUT"
      printf '\n' >> "$OUTPUT"
      ;;
    *)
      printf '%s\n' "$line" >> "$OUTPUT"
      ;;
  esac
done < "$TEMPLATE"

if grep -Eq '__FFMPEG_(JS_GZIP_BASE64|WASM_GZIP_BASE64|RUNTIME)__|__SMOKE_(INPUT_BASE64|TEST_BODY)__' "$OUTPUT"; then
  fail "A smoke-test packaging placeholder remains in $OUTPUT"
fi
[[ -s "$OUTPUT" ]] || fail "Smoke-test HTML was not produced"
log "Smoke-test HTML packaged: $OUTPUT ($(bytes_of "$OUTPUT") bytes)"
