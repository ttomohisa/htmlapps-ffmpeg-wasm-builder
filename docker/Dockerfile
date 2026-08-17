# syntax=docker/dockerfile:1.7

ARG EMSDK_VERSION=6.0.6
ARG EMSCRIPTEN_COMMIT=ce75e06884093bcefb86a6b8fd56a5d62a4cc245
FROM emscripten/emsdk:${EMSDK_VERSION} AS toolchain-base
ARG EMSDK_VERSION
ARG EMSCRIPTEN_COMMIT
ENV DEBIAN_FRONTEND=noninteractive \
    EMSDK_VERSION=${EMSDK_VERSION} \
    EMSCRIPTEN_COMMIT=${EMSCRIPTEN_COMMIT} \
    INSTALL_DIR=/opt/ffmpeg-wasm \
    SRC_DIR=/src \
    OUT_DIR=/out
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       git ca-certificates pkg-config make autoconf automake libtool
WORKDIR /workspace

# Expensive x264 layer. Profile/runner/runtime edits do not invalidate this stage.
FROM toolchain-base AS x264-builder
ARG X264_REPOSITORY
ARG X264_FALLBACK_REPOSITORY
ARG X264_REF
ARG X264_COMMIT
ENV X264_REPOSITORY=${X264_REPOSITORY} \
    X264_FALLBACK_REPOSITORY=${X264_FALLBACK_REPOSITORY} \
    X264_REF=${X264_REF} \
    X264_COMMIT=${X264_COMMIT}
COPY scripts/docker-common.sh scripts/build-x264.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-x264.sh

# Exact FFmpeg checkout. App source edits leave this stage cached.
FROM toolchain-base AS ffmpeg-source
ARG FFMPEG_REPOSITORY
ARG FFMPEG_REF
ARG FFMPEG_COMMIT
ENV FFMPEG_REPOSITORY=${FFMPEG_REPOSITORY} \
    FFMPEG_REF=${FFMPEG_REF} \
    FFMPEG_COMMIT=${FFMPEG_COMMIT}
COPY scripts/docker-common.sh scripts/fetch-ffmpeg.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/fetch-ffmpeg.sh

# Only profile flags / C runner changes invalidate the expensive FFmpeg library build.
FROM toolchain-base AS wasm-builder
ARG BUILDER_VERSION=1.0.0
ARG FFMPEG_REPOSITORY
ARG FFMPEG_REF
ARG FFMPEG_COMMIT
ARG X264_REPOSITORY
ARG X264_FALLBACK_REPOSITORY
ARG X264_REF
ARG X264_COMMIT
ARG PROFILE=video-compressor
ENV BUILDER_VERSION=${BUILDER_VERSION} \
    FFMPEG_REPOSITORY=${FFMPEG_REPOSITORY} \
    FFMPEG_REF=${FFMPEG_REF} \
    FFMPEG_COMMIT=${FFMPEG_COMMIT} \
    X264_REPOSITORY=${X264_REPOSITORY} \
    X264_FALLBACK_REPOSITORY=${X264_FALLBACK_REPOSITORY} \
    X264_REF=${X264_REF} \
    X264_COMMIT=${X264_COMMIT} \
    PROFILE=${PROFILE}
COPY --from=x264-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
COPY --from=ffmpeg-source /src/ffmpeg /src/ffmpeg
COPY profiles /workspace/profiles
COPY runners /workspace/runners
COPY scripts/docker-common.sh scripts/build-ffmpeg.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-ffmpeg.sh

# Runtime/smoke-test edits should not force FFmpeg/x264 to rebuild.
FROM toolchain-base AS package-builder
ARG PROFILE=video-compressor
ENV PROFILE=${PROFILE} OUT_DIR=/out
COPY --from=wasm-builder /out/ /out/
COPY runtime /workspace/runtime
COPY tests /workspace/tests
COPY scripts/docker-common.sh scripts/pack-smoke-test.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/pack-smoke-test.sh

FROM scratch AS export
COPY --from=package-builder /out/ /
