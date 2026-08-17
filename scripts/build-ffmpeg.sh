#!/usr/bin/env bash
set -euo pipefail
source /workspace/scripts/docker-common.sh
require_build_env
load_profile_flags
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
log "Configuring FFmpeg libraries for the single-thread browser runner"
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

for feature in \
  CONFIG_LIBX264 CONFIG_LIBX264_ENCODER CONFIG_AAC_ENCODER \
  CONFIG_H264_DECODER CONFIG_HEVC_DECODER CONFIG_MOV_DEMUXER \
  CONFIG_MP4_MUXER CONFIG_FILE_PROTOCOL CONFIG_SCALE_FILTER CONFIG_ARESAMPLE_FILTER; do
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

log "Building FFmpeg static libraries"
emmake make -j"$JOBS"

for lib in \
  libavfilter/libavfilter.a \
  libavformat/libavformat.a \
  libavcodec/libavcodec.a \
  libswresample/libswresample.a \
  libswscale/libswscale.a \
  libavutil/libavutil.a; do
  [[ -s "$lib" ]] || fail "Expected FFmpeg library is missing: $lib"
done

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
  -sEXPORTED_RUNTIME_METHODS=FS,callMain \
  -sERROR_ON_UNDEFINED_SYMBOLS=1 \
  -Wl,--start-group \
    libavfilter/libavfilter.a \
    libavformat/libavformat.a \
    libavcodec/libavcodec.a \
    libswresample/libswresample.a \
    libswscale/libswscale.a \
    libavutil/libavutil.a \
    "$INSTALL_DIR/lib/libx264.a" \
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
  "schemaVersion": 4,
  "builderVersion": "$BUILDER_VERSION",
  "profile": "$PROFILE",
  "versions": {
    "emscripten": "$EMSDK_VERSION",
    "emscriptenCommit": "$EMSCRIPTEN_COMMIT",
    "ffmpegRef": "$FFMPEG_REF",
    "ffmpegCommit": "$FFMPEG_COMMIT",
    "x264Ref": "$X264_REF",
    "x264Commit": "$X264_COMMIT"
  },
  "runtime": {
    "frontend": "public-libav-runner",
    "runnerApiVersion": 1,
    "threading": "none",
    "factory": "createFFmpegCore",
    "requiresSharedArrayBuffer": false,
    "requiresCrossOriginIsolation": false,
    "fileProtocolSingleHtml": true
  },
  "capabilities": {
    "output": "H.264 + AAC MP4",
    "resize": true,
    "fps": true,
    "crf": true,
    "videoBitrate": true,
    "audioBitrate": true,
    "dropAudio": true,
    "arbitraryFfmpegArgs": false
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
