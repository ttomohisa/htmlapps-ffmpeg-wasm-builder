#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_x264_env

JOBS="${JOBS:-$(nproc)}"
mkdir -p "$SRC_DIR" "$INSTALL_DIR"
print_toolchain "x264"

log "Fetching x264"
clone_exact_commit "$X264_REPOSITORY" "$X264_FALLBACK_REPOSITORY" "$X264_COMMIT" "$SRC_DIR/x264"

# Keep x264 single-threaded so the browser runtime stays SharedArrayBuffer-free.
log "Building x264 (static, asm/opencl/thread disabled)"
pushd "$SRC_DIR/x264" >/dev/null
export CFLAGS="-O3 -fPIC"
export CXXFLAGS="$CFLAGS"
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --host=x86-gnu \
  --enable-static \
  --disable-cli \
  --disable-asm \
  --disable-opencl \
  --disable-thread \
  --bit-depth=8 \
  --chroma-format=420 \
  --extra-cflags="$CFLAGS"
emmake make -j"$JOBS"
emmake make install-lib-static
popd >/dev/null
