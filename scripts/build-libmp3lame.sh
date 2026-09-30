#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_libmp3lame_env
print_toolchain "libmp3lame"

JOBS="${JOBS:-$(nproc)}"
archive="$SRC_DIR/lame-${LIBMP3LAME_VERSION}.tar.gz"
source_dir="$SRC_DIR/lame-${LIBMP3LAME_VERSION}"

rm -rf "$source_dir"
mkdir -p "$SRC_DIR"
log "Downloading LAME ${LIBMP3LAME_VERSION}"
curl --fail --location --retry 3 --retry-delay 2 "$LIBMP3LAME_URL" -o "$archive"
printf '%s  %s\n' "$LIBMP3LAME_SHA256" "$archive" | sha256sum -c -
tar -xzf "$archive" -C "$SRC_DIR"
[[ -d "$source_dir" ]] || fail "LAME source directory was not extracted: $source_dir"

pushd "$source_dir" >/dev/null
log "Building LAME ${LIBMP3LAME_VERSION} for WebAssembly"
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --disable-shared \
  --enable-static \
  --disable-frontend \
  --disable-decoder \
  --disable-analyzer-hooks \
  --disable-nasm \
  --disable-cpml
emmake make -j"$JOBS"
emmake make install
popd >/dev/null

[[ -s "$INSTALL_DIR/lib/libmp3lame.a" ]] || fail "libmp3lame.a was not produced"
[[ -s "$INSTALL_DIR/include/lame/lame.h" ]] || fail "lame.h was not installed"
log "LAME build completed"
