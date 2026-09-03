#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_build_env
load_profile_flags
load_profile_config
print_toolchain "wasm"

JOBS="${JOBS:-$(nproc)}"
RUNNER_SOURCE="/workspace/runners/${PROFILE}.c"
[[ -f "$RUNNER_SOURCE" ]] || fail "Runner not found: $RUNNER_SOURCE"
mkdir -p "$OUT_DIR"

export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
export CFLAGS="-O3 -I$INSTALL_DIR/include"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$INSTALL_DIR/lib"

pushd "$SRC_DIR/ffmpeg" >/dev/null
log "Configuring FFmpeg libraries for profile: $PROFILE_DISPLAY_NAME"
emconfigure ./configure \
  --target-os=none \
  --arch=x86_32 \
  --enable-cross-compile \
  --disable-asm \
  --disable-stripping \
  --disable-doc \
  --disable-debug \
  --disable-checkasm \
  --disable-runtime-cpudetect \
  --disable-autodetect \
  --disable-network \
  --disable-iconv \
  --disable-pthreads \
  --disable-w32threads \
  --disable-os2threads \
  --disable-programs \
  --disable-avdevice \
  --nm=emnm --ar=emar --ranlib=emranlib \
  --cc=emcc --cxx=em++ --objcc=emcc --dep-cc=emcc --ld=emcc \
  --extra-cflags="$CFLAGS" \
  --extra-cxxflags="$CXXFLAGS" \
  --extra-ldflags="$LDFLAGS" \
  "${PROFILE_FLAGS[@]}"

for feature in "${PROFILE_REQUIRED_CONFIG[@]}"; do
  assert_ffmpeg_config "$feature"
done

if grep -q '^HAVE_PTHREADS=yes$' ffbuild/config.mak; then
  fail "The browser build unexpectedly enabled pthreads"
fi
if grep -q '^HAVE_THREADS=yes$' ffbuild/config.mak; then
  fail "The browser build unexpectedly enabled a thread backend"
fi
if grep -q '^CONFIG_FFMPEG=yes$' ffbuild/config.mak; then
  fail "The upstream ffmpeg CLI must stay disabled; this builder links the public-libav runner only"
fi
if [[ "$PROFILE_USE_X264" == "0" ]] && grep -q '^CONFIG_LIBX264=yes$' ffbuild/config.mak; then
  fail "Profile $PROFILE must not enable libx264"
fi
if [[ "$PROFILE_USE_LIBWEBP" == "0" ]] && grep -q '^CONFIG_LIBWEBP=yes$' ffbuild/config.mak; then
  fail "Profile $PROFILE must not enable libwebp"
fi
if [[ "$PROFILE_USE_LIBVPX" == "0" ]] && grep -q '^CONFIG_LIBVPX=yes$' ffbuild/config.mak; then
  fail "Profile $PROFILE must not enable libvpx"
fi
if [[ "$PROFILE_USE_LIBOPUS" == "0" ]] && grep -q '^CONFIG_LIBOPUS=yes$' ffbuild/config.mak; then
  fail "Profile $PROFILE must not enable libopus"
fi

log "Building FFmpeg static libraries"
emmake make -j"$JOBS"

for lib in "${PROFILE_LINK_LIBS[@]}"; do
  [[ -s "$lib" ]] || fail "Expected FFmpeg library is missing: $lib"
done

link_inputs=("${PROFILE_LINK_LIBS[@]}")
if [[ "$PROFILE_USE_X264" == "1" ]]; then
  [[ -s "$INSTALL_DIR/lib/libx264.a" ]] || fail "Profile requires x264 but libx264.a is missing"
  link_inputs+=("$INSTALL_DIR/lib/libx264.a")
fi
if [[ "$PROFILE_USE_LIBVPX" == "1" ]]; then
  [[ -s "$INSTALL_DIR/lib/libvpx.a" ]] || fail "Profile requires libvpx but libvpx.a is missing"
  link_inputs+=("$INSTALL_DIR/lib/libvpx.a")
fi
if [[ "$PROFILE_USE_LIBOPUS" == "1" ]]; then
  [[ -s "$INSTALL_DIR/lib/libopus.a" ]] || fail "Profile requires Opus but libopus.a is missing"
  link_inputs+=("$INSTALL_DIR/lib/libopus.a")
fi
if [[ "$PROFILE_USE_LIBWEBP" == "1" ]]; then
  for lib in libwebpmux.a libwebp.a libsharpyuv.a; do
    [[ -s "$INSTALL_DIR/lib/$lib" ]] || fail "Profile requires libwebp but $lib is missing"
  done
  # Static dependency order matters: mux -> codec -> sharpyuv.
  link_inputs+=(
    "$INSTALL_DIR/lib/libwebpmux.a"
    "$INSTALL_DIR/lib/libwebp.a"
    "$INSTALL_DIR/lib/libsharpyuv.a"
  )
fi

runtime_methods="FS,callMain"
extra_runtime_libs=()
if [[ "$PROFILE_USE_WORKERFS" == "1" ]]; then
  runtime_methods+=",WORKERFS"
  extra_runtime_libs+=("-lworkerfs.js")
fi

log "Linking the profile runner"
emcc "$RUNNER_SOURCE" \
  -I. -I"$INSTALL_DIR/include" \
  -Oz \
  -sMODULARIZE=1 \
  -sWASM_BIGINT=1 \
  -sEXPORT_NAME=createFFmpegCore \
  -sINVOKE_RUN=0 \
  -sEXIT_RUNTIME=0 \
  -sFORCE_FILESYSTEM=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_HEAP=67108864 \
  -sMAXIMUM_MEMORY=2147483648 \
  -sSTACK_SIZE=5242880 \
  -sENVIRONMENT=worker \
  -sINCOMING_MODULE_JS_API=wasmBinary,instantiateWasm,locateFile,print,printErr \
  "-sEXPORTED_RUNTIME_METHODS=${runtime_methods}" \
  "${extra_runtime_libs[@]}" \
  -sERROR_ON_UNDEFINED_SYMBOLS=1 \
  -Wl,--start-group \
  "${link_inputs[@]}" \
  -Wl,--end-group \
  -o "$OUT_DIR/ffmpeg.js"
popd >/dev/null

[[ -s "$OUT_DIR/ffmpeg.js" ]] || fail "ffmpeg.js was not produced"
[[ -s "$OUT_DIR/ffmpeg.wasm" ]] || fail "ffmpeg.wasm was not produced"
gzip -9 -c "$OUT_DIR/ffmpeg.js" > "$OUT_DIR/ffmpeg.js.gz"
gzip -9 -c "$OUT_DIR/ffmpeg.wasm" > "$OUT_DIR/ffmpeg.wasm.gz"
validate_wasm "$OUT_DIR/ffmpeg.wasm"
grep -q 'createFFmpegCore' "$OUT_DIR/ffmpeg.js" || fail "createFFmpegCore factory not found"
for gz in "$OUT_DIR"/*.gz; do gzip -t "$gz"; done

cat > "$OUT_DIR/manifest.json" <<EOF_JSON
{
  "schemaVersion": 7,
  "builderVersion": "$BUILDER_VERSION",
  "profile": "$PROFILE",
  "displayName": "$PROFILE_DISPLAY_NAME",
  "binaryLicense": "$PROFILE_BINARY_LICENSE",
  "versions": {
    "emscripten": "$EMSDK_VERSION",
    "emscriptenCommit": "$EMSCRIPTEN_COMMIT",
    "ffmpegRef": "$FFMPEG_REF",
    "ffmpegCommit": "$FFMPEG_COMMIT",
    "x264Ref": "$X264_REF",
    "x264Commit": "$X264_COMMIT",
    "x264Linked": $([[ "$PROFILE_USE_X264" == "1" ]] && echo true || echo false),
    "libwebpRef": "$LIBWEBP_REF",
    "libwebpCommit": "$LIBWEBP_COMMIT",
    "libwebpLinked": $([[ "$PROFILE_USE_LIBWEBP" == "1" ]] && echo true || echo false),
    "libvpxRef": "$LIBVPX_REF",
    "libvpxCommit": "$LIBVPX_COMMIT",
    "libvpxLinked": $([[ "$PROFILE_USE_LIBVPX" == "1" ]] && echo true || echo false),
    "libopusRef": "$LIBOPUS_REF",
    "libopusCommit": "$LIBOPUS_COMMIT",
    "libopusLinked": $([[ "$PROFILE_USE_LIBOPUS" == "1" ]] && echo true || echo false)
  },
  "runtime": {
    "frontend": "public-libav-runner",
    "runnerApiVersion": 1,
    "threading": "none",
    "factory": "createFFmpegCore",
    "requiresSharedArrayBuffer": false,
    "requiresCrossOriginIsolation": false,
    "fileProtocolSingleHtml": true,
    "workerFsInput": $([[ "$PROFILE_USE_WORKERFS" == "1" ]] && echo true || echo false)
  },
  "capabilities": $PROFILE_CAPABILITIES_JSON,
  "files": {
    "ffmpeg.js": { "bytes": $(bytes_of "$OUT_DIR/ffmpeg.js"), "sha256": "$(sha256_of "$OUT_DIR/ffmpeg.js")" },
    "ffmpeg.wasm": { "bytes": $(bytes_of "$OUT_DIR/ffmpeg.wasm"), "sha256": "$(sha256_of "$OUT_DIR/ffmpeg.wasm")" },
    "ffmpeg.js.gz": { "bytes": $(bytes_of "$OUT_DIR/ffmpeg.js.gz") },
    "ffmpeg.wasm.gz": { "bytes": $(bytes_of "$OUT_DIR/ffmpeg.wasm.gz") }
  }
}
EOF_JSON

log "WASM build completed"
du -h "$OUT_DIR"/* | sort -h
