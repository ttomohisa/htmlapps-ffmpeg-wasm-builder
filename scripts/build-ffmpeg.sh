#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_build_env
load_profile_flags
load_profile_config
print_toolchain "wasm"

JOBS="${JOBS:-$(nproc)}"
THREADING_MODE="${THREADING_MODE:-single-thread}"
RUNNER_SOURCE="/workspace/runners/${PROFILE}.c"
[[ -f "$RUNNER_SOURCE" ]] || fail "Runner not found: $RUNNER_SOURCE"
mkdir -p "$OUT_DIR"

export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
profile_compile_flags=()
profile_link_flags=()
if [[ "$PROFILE_USE_ZLIB" == "1" ]]; then
  # FFmpeg's native PNG decoder selects inflate_wrapper, which requires zlib.
  # Emscripten provides zlib as a system port; enable it for configure tests,
  # FFmpeg compilation, and the final public-libav runner link.
  profile_compile_flags+=("-sUSE_ZLIB=1")
  profile_link_flags+=("-sUSE_ZLIB=1")
fi

profile_compile_flags_text="${profile_compile_flags[*]:-}"
profile_link_flags_text="${profile_link_flags[*]:-}"
if [[ "$THREADING_MODE" == "multi-thread" ]]; then
  export CFLAGS="-O3 -pthread ${profile_compile_flags_text} -I$INSTALL_DIR/include"
  export CXXFLAGS="$CFLAGS"
  export LDFLAGS="-pthread ${profile_link_flags_text} -L$INSTALL_DIR/lib"
  thread_config=(--enable-pthreads --disable-w32threads --disable-os2threads)
else
  export CFLAGS="-O3 ${profile_compile_flags_text} -I$INSTALL_DIR/include"
  export CXXFLAGS="$CFLAGS"
  export LDFLAGS="${profile_link_flags_text} -L$INSTALL_DIR/lib"
  thread_config=(--disable-pthreads --disable-w32threads --disable-os2threads)
fi

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
  "${thread_config[@]}" \
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

if [[ "$THREADING_MODE" == "multi-thread" ]]; then
  grep -q '^HAVE_PTHREADS=yes$' ffbuild/config.mak \
    || fail "The multi-thread browser build did not enable pthreads"
  grep -q '^HAVE_THREADS=yes$' ffbuild/config.mak \
    || fail "The multi-thread browser build did not enable a thread backend"
else
  if grep -q '^HAVE_PTHREADS=yes$' ffbuild/config.mak; then
    fail "The single-thread browser build unexpectedly enabled pthreads"
  fi
  if grep -q '^HAVE_THREADS=yes$' ffbuild/config.mak; then
    fail "The single-thread browser build unexpectedly enabled a thread backend"
  fi
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
if [[ "$PROFILE_USE_ZLIB" == "1" ]]; then
  grep -q '^CONFIG_ZLIB=yes$' ffbuild/config.mak \
    || fail "Profile $PROFILE requires zlib, but FFmpeg configure did not enable it"
else
  if grep -q '^CONFIG_ZLIB=yes$' ffbuild/config.mak; then
    fail "Profile $PROFILE unexpectedly enabled zlib"
  fi
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

log "Linking the profile runner ($THREADING_MODE)"
thread_link_flags=()
thread_defines=(-DFFMPEG_WASM_PTHREADS=0 -DFFMPEG_WASM_DECODER_THREAD_COUNT=1 -DFFMPEG_WASM_ENCODER_THREAD_COUNT=1 -DFFMPEG_WASM_X264_LOOKAHEAD_THREAD_COUNT=1)
if [[ "$THREADING_MODE" == "multi-thread" ]]; then
  thread_defines=(
    -DFFMPEG_WASM_PTHREADS=1
    "-DFFMPEG_WASM_DECODER_THREAD_COUNT=${PROFILE_DECODER_THREAD_COUNT}"
    "-DFFMPEG_WASM_ENCODER_THREAD_COUNT=${PROFILE_ENCODER_THREAD_COUNT}"
    "-DFFMPEG_WASM_X264_LOOKAHEAD_THREAD_COUNT=${PROFILE_X264_LOOKAHEAD_THREAD_COUNT}"
  )
  thread_link_flags=(
    -pthread
    "-sPTHREAD_POOL_SIZE=${PROFILE_PTHREAD_POOL_SIZE}"
    -sPTHREAD_POOL_SIZE_STRICT=2
    -sDEFAULT_PTHREAD_STACK_SIZE=1048576
    -sINITIAL_MEMORY=134217728
  )
else
  thread_link_flags=(-sINITIAL_HEAP=67108864)
fi

emcc "$RUNNER_SOURCE" \
  -I. -I"$INSTALL_DIR/include" \
  "${thread_defines[@]}" \
  -Oz \
  -sMODULARIZE=1 \
  -sWASM_BIGINT=1 \
  -sEXPORT_NAME=createFFmpegCore \
  -sINVOKE_RUN=0 \
  -sEXIT_RUNTIME=0 \
  -sFORCE_FILESYSTEM=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sMAXIMUM_MEMORY=2147483648 \
  "${thread_link_flags[@]}" \
  "${profile_link_flags[@]}" \
  -sSTACK_SIZE=5242880 \
  -sENVIRONMENT=worker \
  -sINCOMING_MODULE_JS_API=wasmBinary,instantiateWasm,locateFile,mainScriptUrlOrBlob,print,printErr \
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
# Emscripten 6.x non-ESM pthread builds reuse the generated main JS as the
# pthread Worker script. No separate *.worker.js asset is expected.
if [[ -e "$OUT_DIR/ffmpeg.worker.js" || -e "$OUT_DIR/ffmpeg.worker.js.gz" ]]; then
  fail "Unexpected legacy pthread worker asset was produced; Emscripten 6.x should reuse ffmpeg.js via mainScriptUrlOrBlob"
fi
gzip -9 -c "$OUT_DIR/ffmpeg.js" > "$OUT_DIR/ffmpeg.js.gz"
gzip -9 -c "$OUT_DIR/ffmpeg.wasm" > "$OUT_DIR/ffmpeg.wasm.gz"
validate_wasm "$OUT_DIR/ffmpeg.wasm"
grep -q 'createFFmpegCore' "$OUT_DIR/ffmpeg.js" || fail "createFFmpegCore factory not found"
for gz in "$OUT_DIR"/*.gz; do gzip -t "$gz"; done

cat > "$OUT_DIR/manifest.json" <<EOF_JSON
{
  "schemaVersion": 8,
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
    "zlibLinked": $([[ "$PROFILE_USE_ZLIB" == "1" ]] && echo true || echo false),
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
    "threading": "$THREADING_MODE",
    "factory": "createFFmpegCore",
    "pthreadPoolSize": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo "$PROFILE_PTHREAD_POOL_SIZE" || echo 0),
    "decoderThreadCount": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo "$PROFILE_DECODER_THREAD_COUNT" || echo 1),
    "encoderThreadCount": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo "$PROFILE_ENCODER_THREAD_COUNT" || echo 1),
    "x264LookaheadThreadCount": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo "$PROFILE_X264_LOOKAHEAD_THREAD_COUNT" || echo 0),
    "requiresSharedArrayBuffer": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo true || echo false),
    "requiresCrossOriginIsolation": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo true || echo false),
    "fileProtocolSingleHtml": $([[ "$THREADING_MODE" == "multi-thread" ]] && echo false || echo true),
    "workerFsInput": $([[ "$PROFILE_USE_WORKERFS" == "1" ]] && echo true || echo false),
    "pthreadWorkerStrategy": "$([[ "$THREADING_MODE" == "multi-thread" ]] && echo main-script-url-or-blob || echo none)"
  },
  "capabilities": $PROFILE_CAPABILITIES_JSON,
  "catalog": {
    "filters": $PROFILE_FILTERS_JSON,
    "encoders": $PROFILE_ENCODERS_JSON,
    "decoders": $PROFILE_DECODERS_JSON,
    "muxers": $PROFILE_MUXERS_JSON,
    "demuxers": $PROFILE_DEMUXERS_JSON
  },
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
