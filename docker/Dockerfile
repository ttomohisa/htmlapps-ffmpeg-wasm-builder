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

# Built only by profiles whose final Wasm actually links x264.
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

# Built only by the animated-WebP profile. Keep libwebp out of every other core.
FROM toolchain-base AS libwebp-builder
ARG LIBWEBP_REPOSITORY
ARG LIBWEBP_FALLBACK_REPOSITORY
ARG LIBWEBP_REF
ARG LIBWEBP_COMMIT
ENV LIBWEBP_REPOSITORY=${LIBWEBP_REPOSITORY} \
    LIBWEBP_FALLBACK_REPOSITORY=${LIBWEBP_FALLBACK_REPOSITORY} \
    LIBWEBP_REF=${LIBWEBP_REF} \
    LIBWEBP_COMMIT=${LIBWEBP_COMMIT}
COPY scripts/docker-common.sh scripts/build-libwebp.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-libwebp.sh

# Built only by the video-compressor profile for VP9 output.
FROM toolchain-base AS libvpx-builder
ARG LIBVPX_REPOSITORY
ARG LIBVPX_FALLBACK_REPOSITORY
ARG LIBVPX_REF
ARG LIBVPX_COMMIT
ENV LIBVPX_REPOSITORY=${LIBVPX_REPOSITORY} \
    LIBVPX_FALLBACK_REPOSITORY=${LIBVPX_FALLBACK_REPOSITORY} \
    LIBVPX_REF=${LIBVPX_REF} \
    LIBVPX_COMMIT=${LIBVPX_COMMIT}
COPY scripts/docker-common.sh scripts/build-libvpx.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-libvpx.sh

# Built only by the video-compressor profile for Opus/WebM audio.
FROM toolchain-base AS libopus-builder
ARG LIBOPUS_REPOSITORY
ARG LIBOPUS_FALLBACK_REPOSITORY
ARG LIBOPUS_REF
ARG LIBOPUS_COMMIT
ENV LIBOPUS_REPOSITORY=${LIBOPUS_REPOSITORY} \
    LIBOPUS_FALLBACK_REPOSITORY=${LIBOPUS_FALLBACK_REPOSITORY} \
    LIBOPUS_REF=${LIBOPUS_REF} \
    LIBOPUS_COMMIT=${LIBOPUS_COMMIT}
COPY scripts/docker-common.sh scripts/build-libopus.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-libopus.sh

# Exact FFmpeg checkout shared by all profiles.
FROM toolchain-base AS ffmpeg-source
ARG FFMPEG_REPOSITORY
ARG FFMPEG_REF
ARG FFMPEG_COMMIT
ENV FFMPEG_REPOSITORY=${FFMPEG_REPOSITORY} \
    FFMPEG_REF=${FFMPEG_REF} \
    FFMPEG_COMMIT=${FFMPEG_COMMIT}
COPY scripts/docker-common.sh scripts/fetch-ffmpeg.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/fetch-ffmpeg.sh

# Common profile build inputs. This stage deliberately does not copy x264.
FROM toolchain-base AS wasm-common
ARG BUILDER_VERSION=1.6.0
ARG FFMPEG_REPOSITORY
ARG FFMPEG_REF
ARG FFMPEG_COMMIT
ARG X264_REPOSITORY
ARG X264_FALLBACK_REPOSITORY
ARG X264_REF
ARG X264_COMMIT
ARG LIBWEBP_REPOSITORY
ARG LIBWEBP_FALLBACK_REPOSITORY
ARG LIBWEBP_REF
ARG LIBWEBP_COMMIT
ARG LIBVPX_REPOSITORY
ARG LIBVPX_FALLBACK_REPOSITORY
ARG LIBVPX_REF
ARG LIBVPX_COMMIT
ARG LIBOPUS_REPOSITORY
ARG LIBOPUS_FALLBACK_REPOSITORY
ARG LIBOPUS_REF
ARG LIBOPUS_COMMIT
ARG PROFILE=video-compressor
ENV BUILDER_VERSION=${BUILDER_VERSION} \
    FFMPEG_REPOSITORY=${FFMPEG_REPOSITORY} \
    FFMPEG_REF=${FFMPEG_REF} \
    FFMPEG_COMMIT=${FFMPEG_COMMIT} \
    X264_REPOSITORY=${X264_REPOSITORY} \
    X264_FALLBACK_REPOSITORY=${X264_FALLBACK_REPOSITORY} \
    X264_REF=${X264_REF} \
    X264_COMMIT=${X264_COMMIT} \
    LIBWEBP_REPOSITORY=${LIBWEBP_REPOSITORY} \
    LIBWEBP_FALLBACK_REPOSITORY=${LIBWEBP_FALLBACK_REPOSITORY} \
    LIBWEBP_REF=${LIBWEBP_REF} \
    LIBWEBP_COMMIT=${LIBWEBP_COMMIT} \
    LIBVPX_REPOSITORY=${LIBVPX_REPOSITORY} \
    LIBVPX_FALLBACK_REPOSITORY=${LIBVPX_FALLBACK_REPOSITORY} \
    LIBVPX_REF=${LIBVPX_REF} \
    LIBVPX_COMMIT=${LIBVPX_COMMIT} \
    LIBOPUS_REPOSITORY=${LIBOPUS_REPOSITORY} \
    LIBOPUS_FALLBACK_REPOSITORY=${LIBOPUS_FALLBACK_REPOSITORY} \
    LIBOPUS_REF=${LIBOPUS_REF} \
    LIBOPUS_COMMIT=${LIBOPUS_COMMIT} \
    PROFILE=${PROFILE}
COPY --from=ffmpeg-source /src/ffmpeg /src/ffmpeg
COPY profiles /workspace/profiles
COPY runners /workspace/runners
COPY scripts/docker-common.sh scripts/build-ffmpeg.sh /workspace/scripts/

FROM wasm-common AS wasm-builder-no-x264
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-ffmpeg.sh

FROM wasm-common AS wasm-builder-with-x264
COPY --from=x264-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-ffmpeg.sh

FROM wasm-common AS wasm-builder-with-video-codecs
COPY --from=x264-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
COPY --from=libvpx-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
COPY --from=libopus-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-ffmpeg.sh

FROM wasm-common AS wasm-builder-with-libwebp
COPY --from=libwebp-builder /opt/ffmpeg-wasm /opt/ffmpeg-wasm
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/build-ffmpeg.sh

# Runtime/smoke-test edits should not force FFmpeg to rebuild.
FROM toolchain-base AS package-no-x264
ARG PROFILE=lossless-video-cutter
ENV PROFILE=${PROFILE} OUT_DIR=/out
COPY --from=wasm-builder-no-x264 /out/ /out/
COPY runtime /workspace/runtime
COPY tests /workspace/tests
COPY scripts/docker-common.sh scripts/pack-smoke-test.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/pack-smoke-test.sh

FROM toolchain-base AS package-with-x264
ARG PROFILE=video-compressor
ENV PROFILE=${PROFILE} OUT_DIR=/out
COPY --from=wasm-builder-with-x264 /out/ /out/
COPY runtime /workspace/runtime
COPY tests /workspace/tests
COPY scripts/docker-common.sh scripts/pack-smoke-test.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/pack-smoke-test.sh

FROM toolchain-base AS package-with-video-codecs
ARG PROFILE=video-compressor
ENV PROFILE=${PROFILE} OUT_DIR=/out
COPY --from=wasm-builder-with-video-codecs /out/ /out/
COPY runtime /workspace/runtime
COPY tests /workspace/tests
COPY scripts/docker-common.sh scripts/pack-smoke-test.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/pack-smoke-test.sh

FROM toolchain-base AS package-with-libwebp
ARG PROFILE=video-to-webp
ENV PROFILE=${PROFILE} OUT_DIR=/out
COPY --from=wasm-builder-with-libwebp /out/ /out/
COPY runtime /workspace/runtime
COPY tests /workspace/tests
COPY scripts/docker-common.sh scripts/pack-smoke-test.sh /workspace/scripts/
RUN chmod +x /workspace/scripts/*.sh && /workspace/scripts/pack-smoke-test.sh

FROM scratch AS export-no-x264
COPY --from=package-no-x264 /out/ /

FROM scratch AS export-with-x264
COPY --from=package-with-x264 /out/ /

FROM scratch AS export-with-video-codecs
COPY --from=package-with-video-codecs /out/ /

FROM scratch AS export-with-libwebp
COPY --from=package-with-libwebp /out/ /
