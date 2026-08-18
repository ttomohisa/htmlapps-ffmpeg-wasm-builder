#!/usr/bin/env bash
set -euo pipefail

log() { printf '\n[ffmpeg-wasm-builder] %s\n' "$*"; }
fail() { printf '\n[ERROR] %s\n' "$*" >&2; exit 1; }

require_x264_env() {
  : "${EMSDK_VERSION:?}"
  : "${EMSCRIPTEN_COMMIT:?}"
  : "${X264_REPOSITORY:?}"
  : "${X264_FALLBACK_REPOSITORY:?}"
  : "${X264_REF:?}"
  : "${X264_COMMIT:?}"
  : "${INSTALL_DIR:?}"
  : "${SRC_DIR:?}"
}

require_ffmpeg_env() {
  : "${EMSDK_VERSION:?}"
  : "${EMSCRIPTEN_COMMIT:?}"
  : "${FFMPEG_REPOSITORY:?}"
  : "${FFMPEG_REF:?}"
  : "${FFMPEG_COMMIT:?}"
  : "${SRC_DIR:?}"
}

require_build_env() {
  require_x264_env
  require_ffmpeg_env
  : "${BUILDER_VERSION:?}"
  : "${PROFILE:?}"
  : "${OUT_DIR:?}"
}

clone_exact_commit() {
  local primary="$1"
  local fallback="$2"
  local commit="$3"
  local destination="$4"

  rm -rf "$destination"
  mkdir -p "$destination"
  git -C "$destination" init -q
  git -C "$destination" remote add origin "$primary"

  if ! git -C "$destination" fetch --depth 1 origin "$commit"; then
    if [[ -z "$fallback" ]]; then
      fail "Could not fetch $commit from $primary"
    fi
    log "Primary source failed; trying fallback mirror"
    git -C "$destination" remote set-url origin "$fallback"
    git -C "$destination" fetch --depth 1 origin "$commit"
  fi

  git -C "$destination" checkout -q --detach FETCH_HEAD
  local actual
  actual="$(git -C "$destination" rev-parse HEAD)"
  [[ "$actual" == "$commit" ]] || fail "Commit mismatch: expected $commit, got $actual"
}

load_profile_flags() {
  local path="/workspace/profiles/${PROFILE}/ffmpeg.flags"
  [[ -f "$path" ]] || fail "Profile flags not found: $path"
  mapfile -t PROFILE_FLAGS < <(
    sed -e 's/\r$//' \
        -e 's/[[:space:]]*#.*$//' \
        -e '/^[[:space:]]*$/d' \
        "$path"
  )
}

load_profile_config() {
  local path="/workspace/profiles/${PROFILE}/profile.env"
  [[ -f "$path" ]] || fail "Profile config not found: $path"
  # shellcheck disable=SC1090
  source "$path"
  : "${PROFILE_DISPLAY_NAME:?}"
  : "${PROFILE_USE_X264:?}"
  : "${PROFILE_USE_WORKERFS:?}"
  : "${PROFILE_BINARY_LICENSE:?}"
  : "${PROFILE_OUTPUT_DESCRIPTION:?}"
  : "${PROFILE_CAPABILITIES_JSON:?}"
  [[ "$PROFILE_USE_X264" == "0" || "$PROFILE_USE_X264" == "1" ]] \
    || fail "PROFILE_USE_X264 must be 0 or 1"
  [[ "$PROFILE_USE_WORKERFS" == "0" || "$PROFILE_USE_WORKERFS" == "1" ]] \
    || fail "PROFILE_USE_WORKERFS must be 0 or 1"
  declare -p PROFILE_REQUIRED_CONFIG >/dev/null 2>&1 \
    || fail "PROFILE_REQUIRED_CONFIG array is missing: $path"
  declare -p PROFILE_LINK_LIBS >/dev/null 2>&1 \
    || fail "PROFILE_LINK_LIBS array is missing: $path"
}

print_toolchain() {
  local phase="$1"
  local emcc_line
  log "Toolchain / phase=$phase"
  emcc_line="$(emcc --version | head -n 1)"
  printf '%s\n' "$emcc_line"
  [[ "$emcc_line" == *"${EMSDK_VERSION}"* ]] || fail "Emscripten version mismatch: expected ${EMSDK_VERSION}"
  [[ "$emcc_line" == *"${EMSCRIPTEN_COMMIT}"* ]] || fail "Emscripten commit mismatch: expected ${EMSCRIPTEN_COMMIT}"
  printf 'Builder:        %s\n' "${BUILDER_VERSION:-n/a}"
  printf 'Emscripten:     %s (%s)\n' "${EMSDK_VERSION:-n/a}" "${EMSCRIPTEN_COMMIT:-n/a}"
  printf 'FFmpeg ref:     %s (%s)\n' "${FFMPEG_REF:-n/a}" "${FFMPEG_COMMIT:-n/a}"
  printf 'x264 ref:       %s (%s)\n' "${X264_REF:-n/a}" "${X264_COMMIT:-n/a}"
  printf 'Profile:        %s\n' "${PROFILE:-n/a}"
}

assert_ffmpeg_config() {
  local name="$1"
  grep -q "^${name}=yes$" ffbuild/config.mak \
    || fail "FFmpeg configure did not enable ${name}. Check the current profile against ${FFMPEG_REF}."
}

sha256_of() { sha256sum "$1" | awk '{print $1}'; }
bytes_of() { wc -c < "$1" | tr -d ' '; }
validate_wasm() {
  local path="$1"
  head -c 4 "$path" | od -An -t x1 | tr -d ' \n' | grep -qi '^0061736d$' \
    || fail "WASM magic bytes are invalid: $path"
}
