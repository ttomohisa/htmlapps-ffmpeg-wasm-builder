#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_libvpx_env
print_toolchain "libvpx"

JOBS="${JOBS:-$(nproc)}"
THREADING_MODE="${THREADING_MODE:-single-thread}"
thread_config=(--disable-multithread)
thread_flags=""
if [[ "$THREADING_MODE" == "multi-thread" ]]; then
  thread_config=(--enable-multithread)
  thread_flags="-pthread"
fi
export LDFLAGS="${LDFLAGS:-} $thread_flags"
clone_exact_commit "$LIBVPX_REPOSITORY" "$LIBVPX_FALLBACK_REPOSITORY" "$LIBVPX_COMMIT" "$SRC_DIR/libvpx"

pushd "$SRC_DIR/libvpx" >/dev/null
log "Building libvpx $LIBVPX_REF ($LIBVPX_COMMIT) for WebAssembly"
# Encoder-only and VP9-only; pthreads are enabled only for the explicit MT variant.
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --target=generic-gnu \
  --disable-shared \
  --enable-static \
  "${thread_config[@]}" \
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
  --extra-cflags="-O3 -fPIC $thread_flags"
emmake make -j"$JOBS"
emmake make install
popd >/dev/null

[[ -s "$INSTALL_DIR/lib/libvpx.a" ]] || fail "libvpx.a was not produced"
[[ -s "$INSTALL_DIR/lib/pkgconfig/vpx.pc" ]] || fail "vpx.pc was not produced"
log "libvpx build completed"
