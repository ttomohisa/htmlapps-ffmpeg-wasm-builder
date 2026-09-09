#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TAG="${1:-v$(grep '^BUILDER_VERSION=' "$ROOT/versions.env" | cut -d= -f2-)}"
RELEASE_PROFILES=(video-compressor video-speed-changer lossless-video-cutter media-inspector video-contact-sheet video-to-gif video-to-webp ffmpeg-filter-builder)

# shellcheck disable=SC1091
source "$ROOT/versions.env"

fail() { printf '\n[ERROR] %s\n' "$*" >&2; exit 1; }
log() { printf '\n[release] %s\n' "$*"; }

EXPECTED_TAG="v${BUILDER_VERSION}"
[[ "$TAG" == "$EXPECTED_TAG" ]] || fail "Release tag $TAG does not match BUILDER_VERSION=$BUILDER_VERSION (expected $EXPECTED_TAG)."

for cmd in git tar gzip sha256sum python3 xargs; do
  command -v "$cmd" >/dev/null 2>&1 || fail "Required release tool is missing: $cmd"
done

profile_threading_variants() {
  local profile="$1"
  local PROFILE_THREADING_VARIANTS="single-thread"
  # shellcheck disable=SC1090
  source "$ROOT/profiles/$profile/profile.env"
  printf '%s\n' "$PROFILE_THREADING_VARIANTS"
}

profile_dist() {
  local profile="$1" variant="$2"
  if [[ -d "$ROOT/dist/$profile/$variant" ]]; then
    printf '%s\n' "$ROOT/dist/$profile/$variant"
  else
    printf '%s\n' "$ROOT/dist/$profile"
  fi
}

for profile in "${RELEASE_PROFILES[@]}"; do
  [[ -s "$ROOT/profiles/$profile/profile.env" ]] || fail "Profile metadata is missing: $profile/profile.env"
  IFS=',' read -r -a variants <<< "$(profile_threading_variants "$profile")"
  for variant in "${variants[@]}"; do
    variant="$(printf '%s' "$variant" | xargs)"
    DIST="$(profile_dist "$profile" "$variant")"
    for file in ffmpeg.js ffmpeg.wasm ffmpeg.js.gz ffmpeg.wasm.gz manifest.json smoke-test.html; do
      [[ -s "$DIST/$file" ]] || fail "Build output is missing: $DIST/$file. Run the build + smoke test first."
    done
    if [[ -e "$DIST/ffmpeg.worker.js" || -e "$DIST/ffmpeg.worker.js.gz" ]]; then
      fail "Unexpected legacy pthread worker asset in $DIST; Emscripten 6.x reuses ffmpeg.js via mainScriptUrlOrBlob"
    fi
  done
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
LIBWEBP_SRC="$WORK_DIR/libwebp-${LIBWEBP_REF}"
LIBVPX_SRC="$WORK_DIR/libvpx-${LIBVPX_REF}"
LIBOPUS_SRC="$WORK_DIR/opus-${LIBOPUS_REF}"
fetch_exact "FFmpeg" "$FFMPEG_REPOSITORY" "" "$FFMPEG_COMMIT" "$FFMPEG_SRC"
fetch_exact "x264" "$X264_REPOSITORY" "$X264_FALLBACK_REPOSITORY" "$X264_COMMIT" "$X264_SRC"
fetch_exact "Emscripten" "$EMSCRIPTEN_REPOSITORY" "" "$EMSCRIPTEN_COMMIT" "$EMSCRIPTEN_SRC"
fetch_exact "libwebp" "$LIBWEBP_REPOSITORY" "$LIBWEBP_FALLBACK_REPOSITORY" "$LIBWEBP_COMMIT" "$LIBWEBP_SRC"
fetch_exact "libvpx" "$LIBVPX_REPOSITORY" "$LIBVPX_FALLBACK_REPOSITORY" "$LIBVPX_COMMIT" "$LIBVPX_SRC"
fetch_exact "Opus" "$LIBOPUS_REPOSITORY" "$LIBOPUS_FALLBACK_REPOSITORY" "$LIBOPUS_COMMIT" "$LIBOPUS_SRC"

[[ -s "$FFMPEG_SRC/LICENSE.md" ]] || fail "FFmpeg LICENSE.md missing from source checkout"
[[ -s "$FFMPEG_SRC/COPYING.GPLv2" ]] || fail "FFmpeg COPYING.GPLv2 missing from source checkout"
[[ -s "$FFMPEG_SRC/COPYING.LGPLv2.1" ]] || fail "FFmpeg COPYING.LGPLv2.1 missing from source checkout"
[[ -s "$X264_SRC/COPYING" ]] || fail "x264 COPYING missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/LICENSE" ]] || fail "Emscripten LICENSE missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/libc/musl/COPYRIGHT" ]] || fail "Emscripten musl COPYRIGHT missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/compiler-rt/LICENSE.TXT" ]] || fail "Emscripten compiler-rt LICENSE.TXT missing from source checkout"
[[ -s "$LIBWEBP_SRC/COPYING" ]] || fail "libwebp COPYING missing from source checkout"
[[ -s "$LIBVPX_SRC/LICENSE" ]] || fail "libvpx LICENSE missing from source checkout"
[[ -s "$LIBOPUS_SRC/COPYING" ]] || fail "Opus COPYING missing from source checkout"

write_buildinfo() {
  local profile="$1"
  local variant="$2"
  local output="$3"
  local PROFILE_DISPLAY_NAME="" PROFILE_USE_X264=0 PROFILE_USE_ZLIB=0 PROFILE_USE_FREETYPE=0 PROFILE_USE_HARFBUZZ=0 PROFILE_USE_LIBVPX=0 PROFILE_USE_LIBOPUS=0 PROFILE_USE_LIBWEBP=0 PROFILE_USE_WORKERFS=0 PROFILE_BINARY_LICENSE="" PROFILE_OUTPUT_DESCRIPTION="" PROFILE_CAPABILITIES_JSON="" PROFILE_THREADING_VARIANTS="single-thread" PROFILE_PTHREAD_POOL_SIZE=8 PROFILE_DECODER_THREAD_COUNT=2 PROFILE_ENCODER_THREAD_COUNT=4 PROFILE_X264_LOOKAHEAD_THREAD_COUNT=1
  local -a PROFILE_REQUIRED_CONFIG=() PROFILE_LINK_LIBS=()
  # shellcheck disable=SC1090
  source "$ROOT/profiles/$profile/profile.env"

  {
    echo "FFmpeg WASM Builder release"
    echo "==========================="
    echo "Builder version: $BUILDER_VERSION"
    echo "Release tag: $TAG"
    echo "Profile: $profile"
    echo "Profile display name: $PROFILE_DISPLAY_NAME"
    echo "Threading variant: $variant"
    echo "SharedArrayBuffer required: $([[ "$variant" == "multi-thread" ]] && echo yes || echo no)"
    echo "Cross-origin isolation required: $([[ "$variant" == "multi-thread" ]] && echo yes || echo no)"
    echo "file:// standalone supported: $([[ "$variant" == "multi-thread" ]] && echo no || echo yes)"
    if [[ "$variant" == "multi-thread" ]]; then
      echo "Pthread pool size: $PROFILE_PTHREAD_POOL_SIZE"
      echo "Decoder thread count: $PROFILE_DECODER_THREAD_COUNT"
      echo "Encoder thread count: $PROFILE_ENCODER_THREAD_COUNT"
      echo "x264 lookahead thread count: $PROFILE_X264_LOOKAHEAD_THREAD_COUNT"
    fi
    echo "Generated core license: $PROFILE_BINARY_LICENSE"
    echo "Output: $PROFILE_OUTPUT_DESCRIPTION"
    echo "x264 linked into this profile: $([[ "$PROFILE_USE_X264" == "1" ]] && echo yes || echo no)"
    echo "zlib system port linked into this profile: $([[ "$PROFILE_USE_ZLIB" == "1" ]] && echo yes || echo no)"
    echo "FreeType system port linked into this profile: $([[ "$PROFILE_USE_FREETYPE" == "1" ]] && echo yes || echo no)"
    echo "HarfBuzz system port linked into this profile: $([[ "$PROFILE_USE_HARFBUZZ" == "1" ]] && echo yes || echo no)"
    echo "libvpx linked into this profile: $([[ "$PROFILE_USE_LIBVPX" == "1" ]] && echo yes || echo no)"
    echo "Opus linked into this profile: $([[ "$PROFILE_USE_LIBOPUS" == "1" ]] && echo yes || echo no)"
    echo "libwebp linked into this profile: $([[ "$PROFILE_USE_LIBWEBP" == "1" ]] && echo yes || echo no)"
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
    echo "libwebp ref: $LIBWEBP_REF"
    echo "libwebp commit: $LIBWEBP_COMMIT"
    echo "libwebp repository: $LIBWEBP_REPOSITORY"
    echo "libwebp fallback repository: $LIBWEBP_FALLBACK_REPOSITORY"
    echo
    echo "libvpx ref: $LIBVPX_REF"
    echo "libvpx commit: $LIBVPX_COMMIT"
    echo "libvpx repository: $LIBVPX_REPOSITORY"
    echo "libvpx fallback repository: $LIBVPX_FALLBACK_REPOSITORY"
    echo
    echo "Opus ref: $LIBOPUS_REF"
    echo "Opus commit: $LIBOPUS_COMMIT"
    echo "Opus repository: $LIBOPUS_REPOSITORY"
    echo "Opus fallback repository: $LIBOPUS_FALLBACK_REPOSITORY"
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
ARGS
    if [[ "$variant" == "multi-thread" ]]; then
      cat <<'ARGS'
--enable-pthreads
--disable-w32threads
--disable-os2threads
ARGS
    else
      cat <<'ARGS'
--disable-pthreads
--disable-w32threads
--disable-os2threads
ARGS
    fi
    cat <<'ARGS'
--disable-programs
--disable-avdevice
ARGS
    echo
    echo "Profile FFmpeg configure arguments ($profile):"
    sed -e 's/\r$//' -e 's/[[:space:]]*#.*$//' -e '/^[[:space:]]*$/d' "$ROOT/profiles/$profile/ffmpeg.flags"
    if [[ "$PROFILE_USE_ZLIB" == "1" ]]; then
      echo
      echo "Emscripten zlib system port: -sUSE_ZLIB=1 (used by FFmpeg configure/build and final link)"
    fi
    if [[ "$PROFILE_USE_FREETYPE" == "1" ]]; then
      echo
      echo "Emscripten FreeType port: -sUSE_FREETYPE=1 (drawtext font rasterization)"
    fi
    if [[ "$PROFILE_USE_HARFBUZZ" == "1" ]]; then
      echo "Emscripten HarfBuzz port: -sUSE_HARFBUZZ=1 (drawtext shaping)"
    fi
    if [[ "$PROFILE_USE_X264" == "1" ]]; then
      echo
      echo "x264 configure arguments:"
      cat <<'ARGS'
--host=x86-gnu
--enable-static
--disable-cli
--disable-asm
--disable-opencl
ARGS
      if [[ "$variant" == "single-thread" ]]; then echo "--disable-thread"; else echo "-pthread via CFLAGS/LDFLAGS"; fi
      cat <<'ARGS'
--bit-depth=8
--chroma-format=420
ARGS
    fi
    if [[ "$PROFILE_USE_LIBVPX" == "1" ]]; then
      echo
      echo "libvpx configure: VP9 encoder-only, static, single-threaded; see scripts/build-libvpx.sh"
    fi
    if [[ "$PROFILE_USE_LIBOPUS" == "1" ]]; then
      echo
      echo "Opus configure: static, tests/programs/intrinsics disabled; see scripts/build-libopus.sh"
    fi
    if [[ "$PROFILE_USE_LIBWEBP" == "1" ]]; then
      echo
      echo "libwebp configure arguments:"
      cat <<'ARGS'
--disable-shared
--enable-static
--disable-threading
--disable-libwebpdecoder
--disable-libwebpdemux
--disable-libwebpextras
--enable-libwebpmux
--disable-gl
--disable-png
--disable-jpeg
--disable-tiff
--disable-gif
ARGS
    fi
    echo
    echo "The exact build scripts are included in the corresponding-source archive."
  } > "$output"
}

make_binary_zip() {
  local profile="$1"
  local variant="$2"
  local buildinfo="$3"
  local DIST
  DIST="$(profile_dist "$profile" "$variant")"
  local PROFILE_DISPLAY_NAME="" PROFILE_USE_X264=0 PROFILE_USE_ZLIB=0 PROFILE_USE_FREETYPE=0 PROFILE_USE_HARFBUZZ=0 PROFILE_USE_LIBVPX=0 PROFILE_USE_LIBOPUS=0 PROFILE_USE_LIBWEBP=0 PROFILE_USE_WORKERFS=0 PROFILE_BINARY_LICENSE="" PROFILE_OUTPUT_DESCRIPTION="" PROFILE_CAPABILITIES_JSON="" PROFILE_THREADING_VARIANTS="single-thread" PROFILE_PTHREAD_POOL_SIZE=8 PROFILE_DECODER_THREAD_COUNT=2 PROFILE_ENCODER_THREAD_COUNT=4 PROFILE_X264_LOOKAHEAD_THREAD_COUNT=1
  local -a PROFILE_REQUIRED_CONFIG=() PROFILE_LINK_LIBS=()
  local suffix=""
  [[ -d "$ROOT/dist/$profile/$variant" ]] && suffix="-$variant"
  local binary_dir="$WORK_DIR/binary-$profile$suffix"
  local binary_zip="$RELEASE_DIR/ffmpeg-wasm-${profile}${suffix}-v${BUILDER_VERSION}.zip"
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
  if [[ "$PROFILE_USE_LIBVPX" == "1" ]]; then
    cp "$LIBVPX_SRC/LICENSE" "$binary_dir/LICENSES/libvpx-LICENSE"
    [[ ! -s "$LIBVPX_SRC/PATENTS" ]] || cp "$LIBVPX_SRC/PATENTS" "$binary_dir/LICENSES/libvpx-PATENTS"
  fi
  if [[ "$PROFILE_USE_LIBOPUS" == "1" ]]; then
    cp "$LIBOPUS_SRC/COPYING" "$binary_dir/LICENSES/Opus-COPYING"
  fi
  if [[ "$PROFILE_USE_LIBWEBP" == "1" ]]; then
    cp "$LIBWEBP_SRC/COPYING" "$binary_dir/LICENSES/libwebp-COPYING"
    [[ ! -s "$LIBWEBP_SRC/PATENTS" ]] || cp "$LIBWEBP_SRC/PATENTS" "$binary_dir/LICENSES/libwebp-PATENTS"
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
  IFS=',' read -r -a variants <<< "$(profile_threading_variants "$profile")"
  for variant in "${variants[@]}"; do
    variant="$(printf '%s' "$variant" | xargs)"
    suffix=""
    [[ -d "$ROOT/dist/$profile/$variant" ]] && suffix="-$variant"
    buildinfo="$RELEASE_DIR/BUILDINFO-${profile}${suffix}.txt"
    write_buildinfo "$profile" "$variant" "$buildinfo"
    make_binary_zip "$profile" "$variant" "$buildinfo"
  done
done

SOURCE_ROOT="$WORK_DIR/source-bundle"
mkdir -p "$SOURCE_ROOT"
mv "$FFMPEG_SRC" "$SOURCE_ROOT/ffmpeg-${FFMPEG_REF}"
mv "$X264_SRC" "$SOURCE_ROOT/x264-${X264_COMMIT:0:12}"
mv "$EMSCRIPTEN_SRC" "$SOURCE_ROOT/emscripten-${EMSCRIPTEN_REF}"
mv "$LIBWEBP_SRC" "$SOURCE_ROOT/libwebp-${LIBWEBP_REF}"
mv "$LIBVPX_SRC" "$SOURCE_ROOT/libvpx-${LIBVPX_REF}"
mv "$LIBOPUS_SRC" "$SOURCE_ROOT/opus-${LIBOPUS_REF}"
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
- exact libwebp source at $LIBWEBP_COMMIT (used by animated-WebP profile)
- exact libvpx source at $LIBVPX_COMMIT (used by video-compressor VP9 output)
- exact Opus source at $LIBOPUS_COMMIT (used by video-compressor WebM audio)
- the FFmpeg WASM Builder recipe at version $BUILDER_VERSION
- profile-specific BUILDINFO files

Published binary profiles:
- video-compressor
- video-speed-changer
- lossless-video-cutter
- media-inspector
- video-contact-sheet
- video-to-gif
- video-to-webp
- ffmpeg-filter-builder (single-thread + multi-thread)
EOF_README

SOURCE_TGZ="$RELEASE_DIR/ffmpeg-wasm-sources-v${BUILDER_VERSION}.tar.gz"
tar -C "$WORK_DIR" -czf "$SOURCE_TGZ" "source-bundle"

(
  cd "$RELEASE_DIR"
  mapfile -t checksum_files < <(find . -maxdepth 1 -type f ! -name SHA256SUMS.txt -printf '%f\n' | sort)
  sha256sum "${checksum_files[@]}" > SHA256SUMS.txt
)

log "Release assets prepared"
find "$RELEASE_DIR" -maxdepth 1 -type f -printf '%f %k KB\n' | sort
