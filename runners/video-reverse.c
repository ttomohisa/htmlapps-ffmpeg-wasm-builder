/*
 * Experimental, video-only, memory-bounded reverse chunk runner.
 * Reuses the public-libav Filter Builder core (no upstream ffmpeg CLI).
 *
 * This adapter is deliberately narrow: one input, one short reversed chunk,
 * H.264 MP4, and no audio. The caller is responsible for joining multiple
 * chunks in reverse order after frame/timestamp checks.
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

#define REVERSE_MAX_DURATION 2.0
#define REVERSE_MAX_BUFFER_MIB 96.0

static void reverse_usage(void)
{
    fprintf(stderr,
            "Experimental video-reverse single-chunk runner\n"
            "Usage: --input FILE --output MP4 --start-time SECONDS --duration SECONDS\n"
            "       --width EVEN --height EVEN --fps INTEGER [--crf 0..51]\n"
            "  duration: 0.1..2.0 seconds; one chunk only; output H.264 MP4, no audio.\n"
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

int main(int argc, char **argv)
{
    const char *input = NULL, *output = NULL;
    const char *start_text = NULL, *duration_text = NULL;
    double start = 0, duration = 0, width = 0, height = 0, fps = 0, crf = 30;
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
        fps != (int)fps || crf != (int)crf) {
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
    snprintf(filter, sizeof(filter), "scale=%d:%d:flags=bicubic,fps=fps=%d,reverse,setpts=PTS-STARTPTS",
             (int)width, (int)height, (int)fps);
    snprintf(fps_text, sizeof(fps_text), "%d", (int)fps);
    snprintf(crf_text, sizeof(crf_text), "%d", (int)crf);

    char *delegated[] = {
        argv[0], "--input", (char *)input, "--output", (char *)output,
        "--start-time", (char *)start_text, "--duration", (char *)duration_text,
        "--video-filter", filter, "--fps", fps_text, "--crf", crf_text, "--no-audio"
    };
    return filter_builder_main((int)(sizeof(delegated) / sizeof(delegated[0])), delegated);
}
