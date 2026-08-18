#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TAG="${1:-v$(grep '^BUILDER_VERSION=' "$ROOT/versions.env" | cut -d= -f2-)}"
RELEASE_PROFILES=(video-compressor lossless-video-cutter)

# shellcheck disable=SC1091
source "$ROOT/versions.env"

fail() { printf '\n[ERROR] %s\n' "$*" >&2; exit 1; }
log() { printf '\n[release] %s\n' "$*"; }

EXPECTED_TAG="v${BUILDER_VERSION}"
[[ "$TAG" == "$EXPECTED_TAG" ]] || fail "Release tag $TAG does not match BUILDER_VERSION=$BUILDER_VERSION (expected $EXPECTED_TAG)."

for cmd in git tar gzip sha256sum python3; do
  command -v "$cmd" >/dev/null 2>&1 || fail "Required release tool is missing: $cmd"
done

for profile in "${RELEASE_PROFILES[@]}"; do
  DIST="$ROOT/dist/$profile"
  for file in ffmpeg.js ffmpeg.wasm ffmpeg.js.gz ffmpeg.wasm.gz manifest.json smoke-test.html; do
    [[ -s "$DIST/$file" ]] || fail "Build output is missing: $DIST/$file. Run the build + smoke test first."
  done
  [[ -s "$ROOT/profiles/$profile/profile.env" ]] || fail "Profile metadata is missing: $profile/profile.env"
done

RELEASE_DIR="$ROOT/release"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

fetch_exact() {
  local label="$1"
  local primary="$2"
  local fallback="$3"
  local commit="$4"
  local destination="$5"

  log "Fetching exact $label source: $commit"
  mkdir -p "$destination"
  git -C "$destination" init -q
  git -C "$destination" remote add origin "$primary"
  if ! git -C "$destination" fetch --depth 1 origin "$commit"; then
    [[ -n "$fallback" ]] || fail "Could not fetch $label $commit from $primary"
    log "$label primary source failed; trying fallback mirror"
    git -C "$destination" remote set-url origin "$fallback"
    git -C "$destination" fetch --depth 1 origin "$commit"
  fi
  git -C "$destination" checkout -q --detach FETCH_HEAD
  local actual
  actual="$(git -C "$destination" rev-parse HEAD)"
  [[ "$actual" == "$commit" ]] || fail "$label commit mismatch: expected $commit, got $actual"
  rm -rf "$destination/.git"
}

FFMPEG_SRC="$WORK_DIR/ffmpeg-${FFMPEG_REF}"
X264_SRC="$WORK_DIR/x264-${X264_COMMIT:0:12}"
EMSCRIPTEN_SRC="$WORK_DIR/emscripten-${EMSCRIPTEN_REF}"
fetch_exact "FFmpeg" "$FFMPEG_REPOSITORY" "" "$FFMPEG_COMMIT" "$FFMPEG_SRC"
fetch_exact "x264" "$X264_REPOSITORY" "$X264_FALLBACK_REPOSITORY" "$X264_COMMIT" "$X264_SRC"
fetch_exact "Emscripten" "$EMSCRIPTEN_REPOSITORY" "" "$EMSCRIPTEN_COMMIT" "$EMSCRIPTEN_SRC"

[[ -s "$FFMPEG_SRC/LICENSE.md" ]] || fail "FFmpeg LICENSE.md missing from source checkout"
[[ -s "$FFMPEG_SRC/COPYING.GPLv2" ]] || fail "FFmpeg COPYING.GPLv2 missing from source checkout"
[[ -s "$FFMPEG_SRC/COPYING.LGPLv2.1" ]] || fail "FFmpeg COPYING.LGPLv2.1 missing from source checkout"
[[ -s "$X264_SRC/COPYING" ]] || fail "x264 COPYING missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/LICENSE" ]] || fail "Emscripten LICENSE missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/libc/musl/COPYRIGHT" ]] || fail "Emscripten musl COPYRIGHT missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/compiler-rt/LICENSE.TXT" ]] || fail "Emscripten compiler-rt LICENSE.TXT missing from source checkout"

write_buildinfo() {
  local profile="$1"
  local output="$2"
  local PROFILE_DISPLAY_NAME PROFILE_USE_X264 PROFILE_USE_WORKERFS PROFILE_BINARY_LICENSE PROFILE_OUTPUT_DESCRIPTION PROFILE_CAPABILITIES_JSON
  local -a PROFILE_REQUIRED_CONFIG PROFILE_LINK_LIBS
  # shellcheck disable=SC1090
  source "$ROOT/profiles/$profile/profile.env"

  {
    echo "FFmpeg WASM Builder release"
    echo "==========================="
    echo "Builder version: $BUILDER_VERSION"
    echo "Release tag: $TAG"
    echo "Profile: $profile"
    echo "Profile display name: $PROFILE_DISPLAY_NAME"
    echo "Generated core license: $PROFILE_BINARY_LICENSE"
    echo "Output: $PROFILE_OUTPUT_DESCRIPTION"
    echo "x264 linked into this profile: $([[ "$PROFILE_USE_X264" == "1" ]] && echo yes || echo no)"
    echo "WORKERFS input enabled: $([[ "$PROFILE_USE_WORKERFS" == "1" ]] && echo yes || echo no)"
    echo
    echo "Emscripten Docker toolchain: emscripten/emsdk:$EMSDK_VERSION"
    echo "Emscripten source ref: $EMSCRIPTEN_REF"
    echo "Emscripten source commit: $EMSCRIPTEN_COMMIT"
    echo "Emscripten source repository: $EMSCRIPTEN_REPOSITORY"
    echo
    echo "FFmpeg ref: $FFMPEG_REF"
    echo "FFmpeg commit: $FFMPEG_COMMIT"
    echo "FFmpeg repository: $FFMPEG_REPOSITORY"
    echo
    echo "x264 ref: $X264_REF"
    echo "x264 commit: $X264_COMMIT"
    echo "x264 repository: $X264_REPOSITORY"
    echo "x264 fallback repository: $X264_FALLBACK_REPOSITORY"
    echo
    echo "FFmpeg base configure arguments:"
    cat <<'ARGS'
--target-os=none
--arch=x86_32
--enable-cross-compile
--disable-asm
--disable-stripping
--disable-doc
--disable-debug
--disable-checkasm
--disable-runtime-cpudetect
--disable-autodetect
--disable-network
--disable-iconv
--disable-pthreads
--disable-w32threads
--disable-os2threads
--disable-programs
--disable-avdevice
ARGS
    echo
    echo "Profile FFmpeg configure arguments ($profile):"
    sed -e 's/\r$//' -e 's/[[:space:]]*#.*$//' -e '/^[[:space:]]*$/d' "$ROOT/profiles/$profile/ffmpeg.flags"
    if [[ "$PROFILE_USE_X264" == "1" ]]; then
      echo
      echo "x264 configure arguments:"
      cat <<'ARGS'
--host=x86-gnu
--enable-static
--disable-cli
--disable-asm
--disable-opencl
--disable-thread
--bit-depth=8
--chroma-format=420
ARGS
    fi
    echo
    echo "The exact build scripts are included in the corresponding-source archive."
  } > "$output"
}

make_binary_zip() {
  local profile="$1"
  local buildinfo="$2"
  local DIST="$ROOT/dist/$profile"
  local PROFILE_DISPLAY_NAME PROFILE_USE_X264 PROFILE_USE_WORKERFS PROFILE_BINARY_LICENSE PROFILE_OUTPUT_DESCRIPTION PROFILE_CAPABILITIES_JSON
  local -a PROFILE_REQUIRED_CONFIG PROFILE_LINK_LIBS
  local binary_dir="$WORK_DIR/binary-$profile"
  local binary_zip="$RELEASE_DIR/ffmpeg-wasm-${profile}-v${BUILDER_VERSION}.zip"
  # shellcheck disable=SC1090
  source "$ROOT/profiles/$profile/profile.env"

  mkdir -p "$binary_dir/LICENSES"
  cp "$DIST/ffmpeg.js" "$binary_dir/"
  cp "$DIST/ffmpeg.wasm" "$binary_dir/"
  cp "$DIST/ffmpeg.js.gz" "$binary_dir/"
  cp "$DIST/ffmpeg.wasm.gz" "$binary_dir/"
  cp "$DIST/manifest.json" "$binary_dir/"
  cp "$ROOT/runtime/browser-ffmpeg.js" "$binary_dir/"
  cp "$buildinfo" "$binary_dir/BUILDINFO.txt"
  cp "$ROOT/THIRD_PARTY_NOTICES.md" "$binary_dir/"
  cp "$ROOT/LICENSE" "$binary_dir/LICENSES/Builder-MIT.txt"
  cp "$FFMPEG_SRC/LICENSE.md" "$binary_dir/LICENSES/FFmpeg-LICENSE.md"
  if [[ "$PROFILE_BINARY_LICENSE" == GPL-* ]]; then
    cp "$FFMPEG_SRC/COPYING.GPLv2" "$binary_dir/LICENSES/FFmpeg-COPYING.GPLv2"
  else
    cp "$FFMPEG_SRC/COPYING.LGPLv2.1" "$binary_dir/LICENSES/FFmpeg-COPYING.LGPLv2.1"
  fi
  if [[ "$PROFILE_USE_X264" == "1" ]]; then
    cp "$X264_SRC/COPYING" "$binary_dir/LICENSES/x264-COPYING"
  fi
  cp "$EMSCRIPTEN_SRC/LICENSE" "$binary_dir/LICENSES/Emscripten-LICENSE"
  cp "$EMSCRIPTEN_SRC/system/lib/libc/musl/COPYRIGHT" "$binary_dir/LICENSES/Emscripten-musl-COPYRIGHT"
  cp "$EMSCRIPTEN_SRC/system/lib/compiler-rt/LICENSE.TXT" "$binary_dir/LICENSES/Emscripten-compiler-rt-LICENSE.txt"

  python3 - "$binary_dir" "$binary_zip" <<'PY'
from pathlib import Path
import sys, zipfile
src = Path(sys.argv[1])
out = Path(sys.argv[2])
with zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for path in sorted(p for p in src.rglob('*') if p.is_file()):
        rel = path.relative_to(src).as_posix()
        info = zipfile.ZipInfo(rel, date_time=(1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        zf.writestr(info, path.read_bytes())
PY
}

for profile in "${RELEASE_PROFILES[@]}"; do
  buildinfo="$RELEASE_DIR/BUILDINFO-${profile}.txt"
  write_buildinfo "$profile" "$buildinfo"
  make_binary_zip "$profile" "$buildinfo"
done

SOURCE_ROOT="$WORK_DIR/source-bundle"
mkdir -p "$SOURCE_ROOT"
mv "$FFMPEG_SRC" "$SOURCE_ROOT/ffmpeg-${FFMPEG_REF}"
mv "$X264_SRC" "$SOURCE_ROOT/x264-${X264_COMMIT:0:12}"
mv "$EMSCRIPTEN_SRC" "$SOURCE_ROOT/emscripten-${EMSCRIPTEN_REF}"
cp "$RELEASE_DIR"/BUILDINFO-*.txt "$SOURCE_ROOT/"

BUILDER_COPY="$SOURCE_ROOT/builder-v${BUILDER_VERSION}"
mkdir -p "$BUILDER_COPY"
tar -C "$ROOT" \
  --exclude='./.git' \
  --exclude='./dist' \
  --exclude='./release' \
  --exclude='./.cache' \
  --exclude='*.zip' \
  --exclude='*.tar.gz' \
  -cf - . | tar -C "$BUILDER_COPY" -xf -

cat > "$SOURCE_ROOT/README.txt" <<EOF_README
Corresponding source for FFmpeg WASM Builder $TAG

This archive contains:
- exact FFmpeg source at $FFMPEG_COMMIT
- exact x264 source at $X264_COMMIT (used by profiles that link x264)
- exact Emscripten source at $EMSCRIPTEN_COMMIT
- the FFmpeg WASM Builder recipe at version $BUILDER_VERSION
- profile-specific BUILDINFO files

Published binary profiles:
- video-compressor
- lossless-video-cutter
EOF_README

SOURCE_TGZ="$RELEASE_DIR/ffmpeg-wasm-sources-v${BUILDER_VERSION}.tar.gz"
tar -C "$WORK_DIR" -czf "$SOURCE_TGZ" "source-bundle"

(
  cd "$RELEASE_DIR"
  sha256sum \
    ffmpeg-wasm-video-compressor-v${BUILDER_VERSION}.zip \
    ffmpeg-wasm-lossless-video-cutter-v${BUILDER_VERSION}.zip \
    ffmpeg-wasm-sources-v${BUILDER_VERSION}.tar.gz \
    BUILDINFO-video-compressor.txt \
    BUILDINFO-lossless-video-cutter.txt > SHA256SUMS.txt
)

log "Release assets prepared"
find "$RELEASE_DIR" -maxdepth 1 -type f -printf '%f %k KB\n' | sort
