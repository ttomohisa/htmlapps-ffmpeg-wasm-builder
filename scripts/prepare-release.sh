#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-video-compressor}"
TAG="${2:-v$(grep '^BUILDER_VERSION=' "$ROOT/versions.env" | cut -d= -f2-)}"

# shellcheck disable=SC1091
source "$ROOT/versions.env"

fail() { printf '\n[ERROR] %s\n' "$*" >&2; exit 1; }
log() { printf '\n[release] %s\n' "$*"; }

EXPECTED_TAG="v${BUILDER_VERSION}"
[[ "$TAG" == "$EXPECTED_TAG" ]] || fail "Release tag $TAG does not match BUILDER_VERSION=$BUILDER_VERSION (expected $EXPECTED_TAG)."

for cmd in git tar gzip sha256sum python3; do
  command -v "$cmd" >/dev/null 2>&1 || fail "Required release tool is missing: $cmd"
done

DIST="$ROOT/dist/$PROFILE"
for file in ffmpeg.js ffmpeg.wasm ffmpeg.js.gz ffmpeg.wasm.gz manifest.json smoke-test.html; do
  [[ -s "$DIST/$file" ]] || fail "Build output is missing: $DIST/$file. Run the build + smoke test first."
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
[[ -s "$X264_SRC/COPYING" ]] || fail "x264 COPYING missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/LICENSE" ]] || fail "Emscripten LICENSE missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/libc/musl/COPYRIGHT" ]] || fail "Emscripten musl COPYRIGHT missing from source checkout"
[[ -s "$EMSCRIPTEN_SRC/system/lib/compiler-rt/LICENSE.TXT" ]] || fail "Emscripten compiler-rt LICENSE.TXT missing from source checkout"

BUILDINFO="$RELEASE_DIR/BUILDINFO.txt"
{
  echo "FFmpeg WASM Builder release"
  echo "==========================="
  echo "Builder version: $BUILDER_VERSION"
  echo "Release tag: $TAG"
  echo "Profile: $PROFILE"
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
  echo "Generated FFmpeg/x264 Wasm licensing: GPL-2.0-or-later"
  echo "Builder/runtime source licensing: MIT (subject to preserved third-party notices)"
  echo
  echo "FFmpeg base configure arguments:"
  cat <<'EOF'
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
EOF
  echo
  echo "Profile FFmpeg configure arguments ($PROFILE):"
  sed -e 's/\r$//' -e 's/[[:space:]]*#.*$//' -e '/^[[:space:]]*$/d' "$ROOT/profiles/$PROFILE/ffmpeg.flags"
  echo
  echo "x264 configure arguments:"
  cat <<'EOF'
--host=x86-gnu
--enable-static
--disable-cli
--disable-asm
--disable-opencl
--disable-thread
--bit-depth=8
--chroma-format=420
EOF
  echo
  echo "The exact build scripts are included in the corresponding-source archive."
} > "$BUILDINFO"

BINARY_DIR="$WORK_DIR/binary"
mkdir -p "$BINARY_DIR/LICENSES"
cp "$DIST/ffmpeg.js" "$BINARY_DIR/"
cp "$DIST/ffmpeg.wasm" "$BINARY_DIR/"
cp "$DIST/ffmpeg.js.gz" "$BINARY_DIR/"
cp "$DIST/ffmpeg.wasm.gz" "$BINARY_DIR/"
cp "$DIST/manifest.json" "$BINARY_DIR/"
cp "$ROOT/runtime/browser-ffmpeg.js" "$BINARY_DIR/"
cp "$BUILDINFO" "$BINARY_DIR/"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$BINARY_DIR/"
cp "$ROOT/LICENSE" "$BINARY_DIR/LICENSES/Builder-MIT.txt"
cp "$FFMPEG_SRC/LICENSE.md" "$BINARY_DIR/LICENSES/FFmpeg-LICENSE.md"
cp "$FFMPEG_SRC/COPYING.GPLv2" "$BINARY_DIR/LICENSES/FFmpeg-COPYING.GPLv2"
cp "$X264_SRC/COPYING" "$BINARY_DIR/LICENSES/x264-COPYING"
cp "$EMSCRIPTEN_SRC/LICENSE" "$BINARY_DIR/LICENSES/Emscripten-LICENSE"
cp "$EMSCRIPTEN_SRC/system/lib/libc/musl/COPYRIGHT" "$BINARY_DIR/LICENSES/Emscripten-musl-COPYRIGHT"
cp "$EMSCRIPTEN_SRC/system/lib/compiler-rt/LICENSE.TXT" "$BINARY_DIR/LICENSES/Emscripten-compiler-rt-LICENSE.txt"

BINARY_ZIP="$RELEASE_DIR/ffmpeg-wasm-${PROFILE}-v${BUILDER_VERSION}.zip"
python3 - "$BINARY_DIR" "$BINARY_ZIP" <<'PY'
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

SOURCE_ROOT="$WORK_DIR/source-bundle"
mkdir -p "$SOURCE_ROOT"
cp "$BUILDINFO" "$SOURCE_ROOT/BUILDINFO.txt"
mv "$FFMPEG_SRC" "$SOURCE_ROOT/ffmpeg-${FFMPEG_REF}"
mv "$X264_SRC" "$SOURCE_ROOT/x264-${X264_COMMIT:0:12}"
mv "$EMSCRIPTEN_SRC" "$SOURCE_ROOT/emscripten-${EMSCRIPTEN_REF}"

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

cat > "$SOURCE_ROOT/README.txt" <<EOF
Corresponding source for FFmpeg WASM Builder $TAG / profile $PROFILE

This archive contains:
- exact FFmpeg source at $FFMPEG_COMMIT
- exact x264 source at $X264_COMMIT
- exact Emscripten source at $EMSCRIPTEN_COMMIT
- the FFmpeg WASM Builder recipe at version $BUILDER_VERSION

BUILDINFO.txt is included in this archive and is also published beside it in the GitHub Release.
EOF

SOURCE_TGZ="$RELEASE_DIR/ffmpeg-wasm-sources-v${BUILDER_VERSION}.tar.gz"
tar -C "$WORK_DIR" -czf "$SOURCE_TGZ" "source-bundle"

(
  cd "$RELEASE_DIR"
  sha256sum \
    "$(basename "$BINARY_ZIP")" \
    "$(basename "$SOURCE_TGZ")" \
    BUILDINFO.txt > SHA256SUMS.txt
)

log "Release assets prepared"
find "$RELEASE_DIR" -maxdepth 1 -type f -printf '%f %k KB\n' | sort
