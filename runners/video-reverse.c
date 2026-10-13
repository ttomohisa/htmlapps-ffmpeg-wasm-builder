/*
 * Experimental, video-only, memory-bounded reverse / forward chunk runner.
 * Reuses the public-libav Filter Builder core (no upstream ffmpeg CLI).
 *
 * This adapter is deliberately narrow: one input, one short reversed chunk,
 * H.264 MP4, and no audio. --direction forward emits a short forward
 * chunk for ping-pong composition; default reverse behavior is unchanged.
 * The caller orders and joins chunks after frame/timestamp checks.
 *
 * The embedded core remains the FFmpeg/x264 GPL-2.0-or-later binary.
 * NO mobile/WASM feasibility has been established by the existence of this file.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the proven demux/decode/filter/encode implementation shared. */
#define main filter_builder_main
#include "ffmpeg-filter-builder.c"
#undef main

#include "video-reverse-join.inc"

#define REVERSE_MAX_DURATION 2.0
#define REVERSE_MAX_BUFFER_MIB 96.0

static void reverse_usage(void)
{
    fprintf(stderr,
            "Experimental video-reverse single-chunk runner\n"
            "Usage: --input FILE --output MP4 --start-time SECONDS --duration SECONDS\n"
            "       --width EVEN --height EVEN --fps INTEGER [--frames COUNT] [--crf 0..51]\n"
            "       [--direction reverse|forward] (default reverse)\n"
            "  duration: 0.1..2.0 seconds; one chunk only; output H.264 MP4, no audio.\n"
            "  --frames forces exact output count (2..120, matching fps*duration).\n"
            "  estimate of cached YUV420 frames must not exceed 96 MiB.\n"
            "  input source codec/rotation and actual peak RAM are not guaranteed.\n");
}

static int parse_finite(const char *text, double low, double high, double *out)
{
    char *end = NULL;
    double value;
    if (!text || !*text) return -1;
    errno = 0;
    value = strtod(text, &end);
    if (errno || !end || *end || !isfinite(value) || value < low || value > high)
        return -1;
    *out = value;
    return 0;
}


/* Count encoded H.264 packets instead of trusting container duration.
 * Abort and delete chunks that are a frame short/long. */
static int verify_reverse_frame_count(const char *file, int expected)
{
    AVFormatContext *ctx = NULL;
    AVPacket *pkt = NULL;
    int index = -1, actual = 0, ret;
    if (!file || expected <= 0) return AVERROR(EINVAL);
    ret = avformat_open_input(&ctx, file, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) goto end;
    index = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (index < 0) { ret = index; goto end; }
    pkt = av_packet_alloc();
    if (!pkt) { ret = AVERROR(ENOMEM); goto end; }
    while ((ret = av_read_frame(ctx, pkt)) >= 0) {
        if (pkt->stream_index == index) actual++;
        av_packet_unref(pkt);
        if (actual > 120) { ret = AVERROR_INVALIDDATA; break; }
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret >= 0 && actual != expected) {
        av_log(NULL, AV_LOG_ERROR, "Reverse frame count mismatch: expected=%d actual=%d.\n",
               expected, actual);
        ret = AVERROR_INVALIDDATA;
    }
end:
    av_packet_free(&pkt);
    avformat_close_input(&ctx);
    return ret;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--concat"))
        return reverse_join_chunks(argc, argv);

    const char *input = NULL, *output = NULL;
    const char *start_text = NULL, *duration_text = NULL;
    const char *direction = "reverse";
    double start = 0, duration = 0, width = 0, height = 0, fps = 0, crf = 30, frames = 0;
    char filter[256], fps_text[16], crf_text[16];
    int i;

    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        reverse_usage();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        printf("experimental reverse chunk adapter; built with FFmpeg %s\n", av_version_info());
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *flag = argv[i];
        const char *value;
        if (i + 1 >= argc) { reverse_usage(); return 2; }
        value = argv[++i];
        if (!strcmp(flag, "--input")) input = value;
        else if (!strcmp(flag, "--direction")) {
            if (strcmp(value, "reverse") && strcmp(value, "forward")) {
                fprintf(stderr, "Only reverse and forward chunk directions are supported.\n");
                return 2;
            }
            direction = value;
        }
        else if (!strcmp(flag, "--output")) output = value;
        else if (!strcmp(flag, "--start-time")) start_text = value;
        else if (!strcmp(flag, "--duration")) duration_text = value;
        else if (!strcmp(flag, "--width")) {
            if (parse_finite(value, 16, 2048, &width) < 0) return 2;
        } else if (!strcmp(flag, "--height")) {
            if (parse_finite(value, 16, 2048, &height) < 0) return 2;
        } else if (!strcmp(flag, "--fps")) {
            if (parse_finite(value, 1, 60, &fps) < 0) return 2;
        } else if (!strcmp(flag, "--crf")) {
            if (parse_finite(value, 0, 51, &crf) < 0) return 2;
        } else if (!strcmp(flag, "--frames")) {
            if (parse_finite(value, 2, 120, &frames) < 0) return 2;
        } else {
            fprintf(stderr, "Unsupported reverse option: %s\n", flag);
            return 2;
        }
    }

    if (!input || !*input || !output || !*output || !start_text || !duration_text ||
        parse_finite(start_text, 0, 2592000, &start) < 0 ||
        parse_finite(duration_text, 0.1, REVERSE_MAX_DURATION, &duration) < 0 ||
        width != (int)width || height != (int)height ||
        ((int)width % 2) || ((int)height % 2) ||
        fps != (int)fps || crf != (int)crf || frames != (int)frames ||
        (frames > 0 && fabs(frames - fps * duration) > 0.05)) {
        reverse_usage();
        return 2;
    }

    /*
     * This is only a frame-buffer lower bound. Additional source decoder,
     * filter, encoder, Emscripten heap and browser memory still apply.
     * A nonzero upper bound makes accidental 10-second reverse impossible.
     */
    double estimated_mib = width * height * 1.5 * fps * duration / (1024.0 * 1024.0);
    if (estimated_mib > REVERSE_MAX_BUFFER_MIB) {
        fprintf(stderr, "Reverse chunk rejected: estimated frame cache %.1f MiB exceeds %.0f MiB limit.\n",
                estimated_mib, REVERSE_MAX_BUFFER_MIB);
        return 2;
    }

    /*
     * Filter Builder prepends precise trim and display-matrix autorotation.
     * Decode -> trim -> autorotate -> scale -> fps -> reverse -> reset PTS ->
     * final fps (from --fps) -> format/encoder. Scaling + fps BEFORE reverse
     * bound stored frame size/count; the final fps fixes frame durations.
     */
    /*
     * Identical scale/fps/trim/PTS chain and x264 settings for both
     * directions. For ping-pong, the caller removes return-leg endpoint
     * frames by choosing a frame-exact source window (not packet deletion).
     */
    const char *direction_filter = !strcmp(direction, "reverse")
                                      ? ",reverse,setpts=PTS-STARTPTS"
                                      : ",setpts=PTS-STARTPTS";
    if (frames > 0) {
        snprintf(filter, sizeof(filter),
                 "scale=%d:%d:flags=bicubic,fps=fps=%d,trim=end_frame=%d%s",
                 (int)width, (int)height, (int)fps, (int)frames, direction_filter);
    } else {
        snprintf(filter, sizeof(filter),
                 "scale=%d:%d:flags=bicubic,fps=fps=%d%s",
                 (int)width, (int)height, (int)fps, direction_filter);
    }
    snprintf(fps_text, sizeof(fps_text), "%d", (int)fps);
    snprintf(crf_text, sizeof(crf_text), "%d", (int)crf);

    char *delegated[] = {
        argv[0], "--input", (char *)input, "--output", (char *)output,
        "--start-time", (char *)start_text, "--duration", (char *)duration_text,
        "--video-filter", filter, "--fps", fps_text, "--crf", crf_text, "--no-audio"
    };
    int ret = filter_builder_main((int)(sizeof(delegated) / sizeof(delegated[0])), delegated);
    if (ret == 0 && frames > 0)
        ret = verify_reverse_frame_count(output, (int)frames) < 0 ? 1 : 0;
    if (ret != 0) remove(output); /* Never expose incomplete or miscounted segments. */
    return ret;
}
