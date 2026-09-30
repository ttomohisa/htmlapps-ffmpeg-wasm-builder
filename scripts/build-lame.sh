#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_lame_env
print_toolchain "lame"

JOBS="${JOBS:-$(nproc)}"
ARCHIVE="/tmp/lame-${LAME_VERSION}.tar.gz"
SOURCE_ROOT="$SRC_DIR/lame-${LAME_VERSION}"

log "Fetching LAME ${LAME_VERSION} source tarball"
curl -fL --retry 3 --retry-delay 2 "$LAME_SOURCE_URL" -o "$ARCHIVE"
printf '%s  %s\n' "$LAME_SHA256" "$ARCHIVE" | sha256sum -c -

rm -rf "$SOURCE_ROOT"
tar -xzf "$ARCHIVE" -C "$SRC_DIR"
[[ -f "$SOURCE_ROOT/configure" ]] || fail "LAME source archive did not contain configure"

log "Building LAME ${LAME_VERSION} (static libmp3lame only)"
pushd "$SOURCE_ROOT" >/dev/null
export CFLAGS="-O3 -fPIC"
export CXXFLAGS="$CFLAGS"
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --disable-shared \
  --enable-static \
  --disable-decoder \
  --disable-frontend \
  --disable-gtktest \
  --disable-nasm
emmake make -j"$JOBS"
emmake make install
popd >/dev/null

[[ -s "$INSTALL_DIR/lib/libmp3lame.a" ]] || fail "LAME build did not produce libmp3lame.a"
[[ -s "$INSTALL_DIR/include/lame/lame.h" ]] || fail "LAME build did not install lame.h"
[[ -s "$INSTALL_DIR/lib/pkgconfig/lame.pc" ]] || fail "LAME build did not install lame.pc"
log "LAME build completed"
