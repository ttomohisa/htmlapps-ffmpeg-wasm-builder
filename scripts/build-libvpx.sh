#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_libvpx_env
print_toolchain "libvpx"

JOBS="${JOBS:-$(nproc)}"
clone_exact_commit "$LIBVPX_REPOSITORY" "$LIBVPX_FALLBACK_REPOSITORY" "$LIBVPX_COMMIT" "$SRC_DIR/libvpx"

pushd "$SRC_DIR/libvpx" >/dev/null
log "Building libvpx $LIBVPX_REF ($LIBVPX_COMMIT) for WebAssembly"
# Encoder-only, VP9-only and single-threaded keeps the Browser Kitty core compact.
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --target=generic-gnu \
  --disable-shared \
  --enable-static \
  --disable-multithread \
  --disable-runtime-cpu-detect \
  --enable-small \
  --disable-spatial-resampling \
  --disable-temporal-denoising \
  --disable-vp9-temporal-denoising \
  --disable-postproc \
  --disable-vp9-postproc \
  --disable-error-concealment \
  --disable-vp9-highbitdepth \
  --disable-examples \
  --disable-tools \
  --disable-docs \
  --disable-unit-tests \
  --disable-vp8 \
  --enable-vp9 \
  --disable-vp9-decoder \
  --enable-vp9-encoder \
  --disable-webm-io \
  --disable-libyuv \
  --extra-cflags="-O3 -fPIC"
emmake make -j"$JOBS"
emmake make install
popd >/dev/null

[[ -s "$INSTALL_DIR/lib/libvpx.a" ]] || fail "libvpx.a was not produced"
[[ -s "$INSTALL_DIR/lib/pkgconfig/vpx.pc" ]] || fail "vpx.pc was not produced"
log "libvpx build completed"
