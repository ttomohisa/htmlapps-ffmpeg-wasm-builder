#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_lame_env
print_toolchain "lame"

JOBS="${JOBS:-$(nproc)}"
ARCHIVE="/tmp/lame-${LAME_REF}.tar.gz"
SOURCE="$SRC_DIR/lame"

rm -rf "$SOURCE" "$ARCHIVE"
mkdir -p "$SOURCE"

log "Downloading official LAME ${LAME_REF} source tarball"
curl -fL --retry 4 --retry-delay 2 "$LAME_URL" -o "$ARCHIVE"
printf '%s  %s\n' "$LAME_SHA256" "$ARCHIVE" | sha256sum -c -
tar -xzf "$ARCHIVE" --strip-components=1 -C "$SOURCE"

pushd "$SOURCE" >/dev/null
log "Building libmp3lame ${LAME_REF} for WebAssembly"
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --disable-shared \
  --enable-static \
  --disable-frontend \
  --disable-decoder \
  --disable-analyzer-hooks \
  --disable-nasm
emmake make -j"$JOBS"
emmake make install
popd >/dev/null

[[ -s "$INSTALL_DIR/lib/libmp3lame.a" ]] || fail "libmp3lame.a was not produced"
[[ -s "$INSTALL_DIR/include/lame/lame.h" ]] || fail "lame/lame.h was not installed"
log "LAME build completed"
