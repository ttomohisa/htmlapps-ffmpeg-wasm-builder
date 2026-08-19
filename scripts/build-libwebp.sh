#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_libwebp_env
print_toolchain "libwebp"

JOBS="${JOBS:-$(nproc)}"
log "Fetching libwebp"
clone_exact_commit "$LIBWEBP_REPOSITORY" "$LIBWEBP_FALLBACK_REPOSITORY" "$LIBWEBP_COMMIT" "$SRC_DIR/libwebp"

log "Building libwebp 1.6.0 (static encoder + mux only, threads/SIMD/tools disabled)"
pushd "$SRC_DIR/libwebp" >/dev/null
./autogen.sh
emconfigure ./configure \
  --prefix="$INSTALL_DIR" \
  --disable-shared \
  --enable-static \
  --disable-threading \
  --disable-sse2 \
  --disable-sse4.1 \
  --disable-avx2 \
  --disable-neon \
  --disable-neon-rtcd \
  --disable-libwebpdecoder \
  --disable-libwebpdemux \
  --disable-libwebpextras \
  --enable-libwebpmux \
  --disable-gl \
  --disable-sdl \
  --disable-png \
  --disable-jpeg \
  --disable-tiff \
  --disable-gif \
  --disable-wic

# Build only the libraries FFmpeg's libwebp_anim wrapper needs. Avoid the
# command-line examples/utilities so they never enter the cache or final Wasm.
emmake make -j"$JOBS" -C sharpyuv install
emmake make -j"$JOBS" -C src install
popd >/dev/null

for lib in libwebp.a libwebpmux.a libsharpyuv.a; do
  [[ -s "$INSTALL_DIR/lib/$lib" ]] || fail "libwebp build did not produce $lib"
done
for pc in libwebp.pc libwebpmux.pc; do
  [[ -s "$INSTALL_DIR/lib/pkgconfig/$pc" ]] || fail "libwebp build did not install $pc"
done
log "libwebp build completed"
