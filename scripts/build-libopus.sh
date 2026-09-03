#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_libopus_env
print_toolchain "libopus"

JOBS="${JOBS:-$(nproc)}"
clone_exact_commit "$LIBOPUS_REPOSITORY" "$LIBOPUS_FALLBACK_REPOSITORY" "$LIBOPUS_COMMIT" "$SRC_DIR/opus"

log "Building Opus $LIBOPUS_REF ($LIBOPUS_COMMIT) for WebAssembly"
emcmake cmake -S "$SRC_DIR/opus" -B "$SRC_DIR/opus/build-wasm" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DBUILD_SHARED_LIBS=OFF \
  -DOPUS_BUILD_SHARED_LIBRARY=OFF \
  -DOPUS_BUILD_TESTING=OFF \
  -DBUILD_TESTING=OFF \
  -DOPUS_BUILD_PROGRAMS=OFF \
  -DOPUS_DISABLE_INTRINSICS=ON \
  -DOPUS_DRED=OFF \
  -DOPUS_OSCE=OFF \
  -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON \
  -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF
cmake --build "$SRC_DIR/opus/build-wasm" --parallel "$JOBS"
cmake --install "$SRC_DIR/opus/build-wasm"

[[ -s "$INSTALL_DIR/lib/libopus.a" ]] || fail "libopus.a was not produced"
[[ -s "$INSTALL_DIR/lib/pkgconfig/opus.pc" ]] || fail "opus.pc was not produced"
log "Opus build completed"
