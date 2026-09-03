/*
 * FFmpeg WASM Builder video runner.
 *
 * This file intentionally uses FFmpeg's public libav* APIs rather than the
 * fftools/ffmpeg command-line frontend. Parts of the decode/filter/encode loop
 * are derived from FFmpeg's doc/examples/transcode.c.
 *
 * The FFmpeg example carries the MIT license:
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Scope: one best video stream, optional one best audio stream, H.264/AAC MP4
 * or VP9/Opus WebM, measured stream bitrate inspection, autorotation, and resize/FPS/quality controls. It is deliberately not a drop-in
 * replacement for the full ffmpeg CLI.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>

#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define RUNNER_VERSION "1.6.0"

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    const char *inspect_output_path;
    const char *codec;
    const char *speed;
    const char *preset;
    int crf;
    int video_bitrate_kbps;
    int audio_bitrate_kbps;
    int max_width;
    int max_height;
    double fps;
    int no_audio;
    int allow_upscale;
    int faststart;
} RunnerOptions;

typedef struct StreamContext {
    int input_index;
    int output_index;
    enum AVMediaType type;
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
    AVFrame *dec_frame;
    AVFrame *filtered_frame;
    AVPacket *enc_pkt;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersrc_ctx;
    AVFilterContext *buffersink_ctx;
    int has_display_matrix;
    int32_t display_matrix[9];
    double display_rotation_degrees;
    double autorotate_degrees;
} StreamContext;

typedef struct RunnerContext {
    RunnerOptions options;
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    StreamContext video;
    StreamContext audio;
    int have_video;
    int have_audio;
    int64_t duration_us;
    double last_progress;
} RunnerContext;

static void reset_stream(StreamContext *stream)
{
    memset(stream, 0, sizeof(*stream));
    stream->input_index = -1;
    stream->output_index = -1;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM video runner %s\n"
        "Usage:\n"
        "  %s --input INPUT --output OUTPUT [options]\n"
        "  %s --input INPUT --inspect-output REPORT.json\n\n"
        "Options:\n"
        "  --codec NAME           h264 (MP4) or vp9 (WebM), default h264\n"
        "  --speed NAME           fastest, fast, balanced, quality (default fast)\n"
        "  --max-width N          Maximum display-oriented output width (0 = source)\n"
        "  --max-height N         Maximum display-oriented output height (0 = source)\n"
        "  --allow-upscale        Allow dimensions larger than the source\n"
        "  --fps N                Output FPS (0 = preserve source timing)\n"
        "  --crf N                Encoder quality fallback when bitrate is 0\n"
        "  --video-bitrate N      Video bitrate in kbit/s\n"
        "  --preset NAME          Legacy x264 preset (accepted for compatibility)\n"
        "  --audio-bitrate N      AAC/Opus bitrate in kbit/s (default 128)\n"
        "  --no-audio             Drop audio\n"
        "  --no-faststart         Do not move MP4 metadata to the front\n"
        "  --inspect-output PATH  Measure source stream information and write JSON\n"
        "  --version              Print runner/FFmpeg version\n"
        "  --help                 Show this help\n",
        RUNNER_VERSION, program, program);
}

static int parse_int(const char *value, int minimum, int maximum, int *out)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || !end || *end != '\0' || parsed < minimum || parsed > maximum)
        return -1;
    *out = (int)parsed;
    return 0;
}

static int parse_double(const char *value, double minimum, double maximum, double *out)
{
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(value, &end);
    if (errno || !end || *end != '\0' || !isfinite(parsed) || parsed < minimum || parsed > maximum)
        return -1;
    *out = parsed;
    return 0;
}

static int parse_options(int argc, char **argv, RunnerOptions *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->codec = "h264";
    options->speed = "fast";
    options->preset = NULL;
    options->crf = 28;
    options->audio_bitrate_kbps = 128;
    options->faststart = 1;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;

        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            print_usage(argv[0]);
            return 1;
        }
        if (!strcmp(arg, "--version")) {
            printf("ffmpeg-wasm-runner %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
            return 1;
        }
        if (!strcmp(arg, "--no-audio")) {
            options->no_audio = 1;
            continue;
        }
        if (!strcmp(arg, "--allow-upscale")) {
            options->allow_upscale = 1;
            continue;
        }
        if (!strcmp(arg, "--no-faststart")) {
            options->faststart = 0;
            continue;
        }

        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return -1;
        }
        value = argv[++i];

        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--output")) options->output_path = value;
        else if (!strcmp(arg, "--inspect-output")) options->inspect_output_path = value;
        else if (!strcmp(arg, "--codec")) options->codec = value;
        else if (!strcmp(arg, "--speed")) options->speed = value;
        else if (!strcmp(arg, "--preset")) options->preset = value;
        else if (!strcmp(arg, "--crf")) {
            if (parse_int(value, 0, 51, &options->crf) < 0) goto invalid;
        } else if (!strcmp(arg, "--video-bitrate")) {
            if (parse_int(value, 0, 1000000, &options->video_bitrate_kbps) < 0) goto invalid;
        } else if (!strcmp(arg, "--audio-bitrate")) {
            if (parse_int(value, 8, 1000000, &options->audio_bitrate_kbps) < 0) goto invalid;
        } else if (!strcmp(arg, "--max-width")) {
            if (parse_int(value, 0, 16384, &options->max_width) < 0) goto invalid;
        } else if (!strcmp(arg, "--max-height")) {
            if (parse_int(value, 0, 16384, &options->max_height) < 0) goto invalid;
        } else if (!strcmp(arg, "--fps")) {
            if (parse_double(value, 0.0, 240.0, &options->fps) < 0) goto invalid;
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return -1;
        }
        continue;

invalid:
        fprintf(stderr, "Invalid value for %s: %s\n", arg, value);
        return -1;
    }

    if (!options->input_path || (!options->output_path && !options->inspect_output_path)) {
        fprintf(stderr, "--input and either --output or --inspect-output are required.\n");
        return -1;
    }
    if (options->output_path && options->inspect_output_path) {
        fprintf(stderr, "--output and --inspect-output cannot be used together.\n");
        return -1;
    }
    if (strcmp(options->codec, "h264") && strcmp(options->codec, "vp9")) {
        fprintf(stderr, "--codec must be h264 or vp9.\n");
        return -1;
    }
    if (strcmp(options->speed, "fastest") && strcmp(options->speed, "fast") &&
        strcmp(options->speed, "balanced") && strcmp(options->speed, "quality")) {
        fprintf(stderr, "--speed must be fastest, fast, balanced, or quality.\n");
        return -1;
    }
    return 0;
}

static const AVPacketSideData *codec_side_data(const AVCodecParameters *par,
                                                enum AVPacketSideDataType type)
{
    if (!par || !par->coded_side_data || par->nb_coded_side_data <= 0)
        return NULL;
    return av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, type);
}

/* Match fftools/cmdutils.c rotation normalization used by FFmpeg autorotate. */
static double get_display_rotation_degrees(const int32_t *display_matrix)
{
    double theta = 0.0;
    if (display_matrix)
        theta = round(av_display_rotation_get(display_matrix));
    return isfinite(theta) ? theta : 0.0;
}

/* Match fftools/cmdutils.c get_rotation() used by FFmpeg autorotate. */
static double get_autorotate_degrees(const int32_t *display_matrix)
{
    double theta = 0.0;
    if (display_matrix)
        theta = -round(av_display_rotation_get(display_matrix));
    if (!isfinite(theta))
        return 0.0;
    theta -= 360.0 * floor(theta / 360.0 + 0.9 / 360.0);
    return theta;
}

static void capture_display_matrix(StreamContext *stream, const AVCodecParameters *par)
{
    const AVPacketSideData *side = codec_side_data(par, AV_PKT_DATA_DISPLAYMATRIX);
    stream->has_display_matrix = 0;
    stream->display_rotation_degrees = 0.0;
    stream->autorotate_degrees = 0.0;
    if (!side || side->size < 9 * sizeof(int32_t))
        return;
    memcpy(stream->display_matrix, side->data, 9 * sizeof(int32_t));
    stream->has_display_matrix = 1;
    stream->display_rotation_degrees = get_display_rotation_degrees(stream->display_matrix);
    stream->autorotate_degrees = get_autorotate_degrees(stream->display_matrix);
}

static int rotation_swaps_dimensions(const StreamContext *stream)
{
    return stream->has_display_matrix &&
           (fabs(stream->autorotate_degrees - 90.0) < 1.0 ||
            fabs(stream->autorotate_degrees - 270.0) < 1.0);
}

static void display_source_dimensions(const StreamContext *stream, int *width, int *height)
{
    *width = stream->dec_ctx->width;
    *height = stream->dec_ctx->height;
    if (rotation_swaps_dimensions(stream)) {
        int temp = *width;
        *width = *height;
        *height = temp;
    }
}

static void compute_output_dimensions(const RunnerOptions *options,
                                      int source_width, int source_height,
                                      int *output_width, int *output_height)
{
    double scale = 1.0;
    double width_scale = 1.0;
    double height_scale = 1.0;
    int width;
    int height;

    if (options->max_width > 0)
        width_scale = (double)options->max_width / source_width;
    if (options->max_height > 0)
        height_scale = (double)options->max_height / source_height;

    if (options->max_width > 0 && options->max_height > 0)
        scale = FFMIN(width_scale, height_scale);
    else if (options->max_width > 0)
        scale = width_scale;
    else if (options->max_height > 0)
        scale = height_scale;

    if (!options->allow_upscale)
        scale = FFMIN(scale, 1.0);
    if (scale <= 0.0)
        scale = 1.0;

    width = (int)floor(source_width * scale + 0.5);
    height = (int)floor(source_height * scale + 0.5);

    width = FFMAX(2, width & ~1);
    height = FFMAX(2, height & ~1);

    *output_width = width;
    *output_height = height;
}

static int choose_sample_rate(const AVCodec *codec, int preferred)
{
    const int *rates = NULL;
    int count = 0;
    int i;
    int best = preferred > 0 ? preferred : 48000;
    int best_delta = INT32_MAX;

    if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_RATE,
                                     0, (const void **)&rates, &count) < 0 || !rates || count <= 0)
        return best;

    for (i = 0; i < count; i++) {
        int delta = abs(rates[i] - best);
        if (delta < best_delta) {
            best_delta = delta;
            preferred = rates[i];
        }
    }
    return preferred > 0 ? preferred : rates[0];
}

static enum AVSampleFormat choose_sample_format(const AVCodec *codec)
{
    const enum AVSampleFormat *formats = NULL;
    int count = 0;
    int i;

    if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT,
                                     0, (const void **)&formats, &count) < 0 || !formats || count <= 0)
        return AV_SAMPLE_FMT_FLTP;

    for (i = 0; i < count; i++)
        if (formats[i] == AV_SAMPLE_FMT_FLTP)
            return formats[i];
    return formats[0];
}

static int open_decoder(RunnerContext *ctx, int stream_index, StreamContext *stream)
{
    AVStream *input_stream = ctx->ifmt_ctx->streams[stream_index];
    const AVCodec *decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    int ret;

    if (!decoder) {
        av_log(NULL, AV_LOG_ERROR, "Decoder not found for input stream %d\n", stream_index);
        return AVERROR_DECODER_NOT_FOUND;
    }

    stream->dec_ctx = avcodec_alloc_context3(decoder);
    if (!stream->dec_ctx)
        return AVERROR(ENOMEM);

    ret = avcodec_parameters_to_context(stream->dec_ctx, input_stream->codecpar);
    if (ret < 0)
        return ret;

    stream->dec_ctx->pkt_timebase = input_stream->time_base;
    stream->dec_ctx->thread_count = 1;
    stream->dec_ctx->thread_type = 0;
    if (input_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        stream->dec_ctx->framerate = av_guess_frame_rate(ctx->ifmt_ctx, input_stream, NULL);

    ret = avcodec_open2(stream->dec_ctx, decoder, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open %s decoder\n", decoder->name);
        return ret;
    }

    stream->dec_frame = av_frame_alloc();
    stream->filtered_frame = av_frame_alloc();
    stream->enc_pkt = av_packet_alloc();
    if (!stream->dec_frame || !stream->filtered_frame || !stream->enc_pkt)
        return AVERROR(ENOMEM);

    stream->input_index = stream_index;
    stream->type = input_stream->codecpar->codec_type;
    return 0;
}

static const char *x264_preset_for_speed(const RunnerOptions *options)
{
    if (options->preset && *options->preset)
        return options->preset;
    if (!strcmp(options->speed, "fastest")) return "ultrafast";
    if (!strcmp(options->speed, "balanced")) return "medium";
    if (!strcmp(options->speed, "quality")) return "slow";
    return "veryfast";
}

static const char *vp9_cpu_used_for_speed(const RunnerOptions *options)
{
    if (!strcmp(options->speed, "fastest")) return "8";
    if (!strcmp(options->speed, "balanced")) return "4";
    if (!strcmp(options->speed, "quality")) return "2";
    return "6";
}

static const char *vp9_lag_in_frames_for_speed(const RunnerOptions *options)
{
    /*
     * A small look-ahead materially improves VP9 compression efficiency.
     * Keep the default browser-friendly while allowing the slower modes to
     * spend more memory/time for better compression.
     */
    if (!strcmp(options->speed, "fastest")) return "0";
    if (!strcmp(options->speed, "balanced")) return "16";
    if (!strcmp(options->speed, "quality")) return "25";
    return "8";
}

static int setup_video_output(RunnerContext *ctx)
{
    StreamContext *stream = &ctx->video;
    AVStream *input_stream = ctx->ifmt_ctx->streams[stream->input_index];
    AVStream *output_stream;
    const int use_vp9 = !strcmp(ctx->options.codec, "vp9");
    const char *encoder_name = use_vp9 ? "libvpx-vp9" : "libx264";
    const AVCodec *encoder = avcodec_find_encoder_by_name(encoder_name);
    AVDictionary *encoder_options = NULL;
    AVRational frame_rate;
    int ret;
    int source_width;
    int source_height;
    int width;
    int height;
    char crf_text[16];

    if (!encoder) {
        av_log(NULL, AV_LOG_ERROR, "%s encoder is not available in this WASM build\n", encoder_name);
        return AVERROR_ENCODER_NOT_FOUND;
    }

    output_stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
    if (!output_stream)
        return AVERROR(ENOMEM);

    stream->enc_ctx = avcodec_alloc_context3(encoder);
    if (!stream->enc_ctx)
        return AVERROR(ENOMEM);

    display_source_dimensions(stream, &source_width, &source_height);
    compute_output_dimensions(&ctx->options, source_width, source_height, &width, &height);

    frame_rate = ctx->options.fps > 0.0
        ? av_d2q(ctx->options.fps, 1001000)
        : av_guess_frame_rate(ctx->ifmt_ctx, input_stream, NULL);
    if (frame_rate.num <= 0 || frame_rate.den <= 0)
        frame_rate = (AVRational){30, 1};

    stream->enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->enc_ctx->width = width;
    stream->enc_ctx->height = height;
    stream->enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    stream->enc_ctx->framerate = frame_rate;
    stream->enc_ctx->time_base = av_inv_q(frame_rate);
    stream->enc_ctx->sample_aspect_ratio = (AVRational){1, 1};
    stream->enc_ctx->gop_size = FFMAX(12, (int)av_q2d(frame_rate) * 2);
    stream->enc_ctx->max_b_frames = use_vp9 ? 0 : 2;
    stream->enc_ctx->thread_count = 1;
    stream->enc_ctx->thread_type = 0;

    if (ctx->options.video_bitrate_kbps > 0)
        stream->enc_ctx->bit_rate = (int64_t)ctx->options.video_bitrate_kbps * 1000;
    else {
        snprintf(crf_text, sizeof(crf_text), "%d", ctx->options.crf);
        av_dict_set(&encoder_options, "crf", crf_text, 0);
    }

    if (use_vp9) {
        av_dict_set(&encoder_options, "deadline", "good", 0);
        av_dict_set(&encoder_options, "cpu-used", vp9_cpu_used_for_speed(&ctx->options), 0);
        av_dict_set(&encoder_options, "lag-in-frames", vp9_lag_in_frames_for_speed(&ctx->options), 0);
    } else {
        av_dict_set(&encoder_options, "preset", x264_preset_for_speed(&ctx->options), 0);
    }

    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        stream->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(stream->enc_ctx, encoder, &encoder_options);
    av_dict_free(&encoder_options);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open %s encoder\n", encoder_name);
        return ret;
    }

    ret = avcodec_parameters_from_context(output_stream->codecpar, stream->enc_ctx);
    if (ret < 0)
        return ret;

    output_stream->time_base = stream->enc_ctx->time_base;
    stream->output_index = output_stream->index;

    av_log(NULL, AV_LOG_INFO,
           "video: %dx%d display=%dx%d rotation=%.0f -> %dx%d, %.3f fps, %s, %d kbit/s\n",
           stream->dec_ctx->width, stream->dec_ctx->height,
           source_width, source_height, stream->display_rotation_degrees,
           width, height, av_q2d(frame_rate), encoder_name,
           ctx->options.video_bitrate_kbps);
    return 0;
}

static int setup_audio_output(RunnerContext *ctx)
{
    StreamContext *stream = &ctx->audio;
    AVStream *output_stream;
    const int use_opus = !strcmp(ctx->options.codec, "vp9");
    const AVCodec *encoder = use_opus ? avcodec_find_encoder_by_name("libopus") : avcodec_find_encoder(AV_CODEC_ID_AAC);
    int channels;
    int ret;

    if (!ctx->have_audio)
        return 0;
    if (!encoder)
        return AVERROR_ENCODER_NOT_FOUND;

    output_stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
    if (!output_stream)
        return AVERROR(ENOMEM);

    stream->enc_ctx = avcodec_alloc_context3(encoder);
    if (!stream->enc_ctx)
        return AVERROR(ENOMEM);

    channels = stream->dec_ctx->ch_layout.nb_channels;
    if (channels <= 0)
        channels = 2;
    if (channels > 2)
        channels = 2;

    stream->enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    stream->enc_ctx->sample_rate = choose_sample_rate(encoder, stream->dec_ctx->sample_rate);
    stream->enc_ctx->sample_fmt = choose_sample_format(encoder);
    stream->enc_ctx->time_base = (AVRational){1, stream->enc_ctx->sample_rate};
    stream->enc_ctx->bit_rate = (int64_t)ctx->options.audio_bitrate_kbps * 1000;
    stream->enc_ctx->thread_count = 1;
    stream->enc_ctx->thread_type = 0;
    av_channel_layout_default(&stream->enc_ctx->ch_layout, channels);

    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        stream->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(stream->enc_ctx, encoder, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open %s encoder\n", use_opus ? "libopus" : "AAC");
        return ret;
    }

    ret = avcodec_parameters_from_context(output_stream->codecpar, stream->enc_ctx);
    if (ret < 0)
        return ret;

    output_stream->time_base = stream->enc_ctx->time_base;
    stream->output_index = output_stream->index;
    return 0;
}

static int init_filter(StreamContext *stream, const char *filter_spec)
{
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterGraph *graph = avfilter_graph_alloc();
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *src_ctx = NULL;
    AVFilterContext *sink_ctx = NULL;
    char args[768];
    int ret = 0;

    if (!outputs || !inputs || !graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (stream->type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            ret = AVERROR_FILTER_NOT_FOUND;
            goto end;
        }

        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 stream->dec_ctx->width, stream->dec_ctx->height, stream->dec_ctx->pix_fmt,
                 stream->dec_ctx->pkt_timebase.num, stream->dec_ctx->pkt_timebase.den,
                 stream->dec_ctx->sample_aspect_ratio.num,
                 stream->dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&src_ctx, buffersrc, "in", args, NULL, graph);
        if (ret < 0) goto end;
        sink_ctx = avfilter_graph_alloc_filter(graph, buffersink, "out");
        if (!sink_ctx) { ret = AVERROR(ENOMEM); goto end; }

        /*
         * FFmpeg 8 introduced typed array AVOptions for buffer sinks and
         * FFmpeg 9 removed the deprecated pix_fmts binary alias. Use the
         * public array option so the WASM runner works with current FFmpeg.
         */
        ret = av_opt_set_array(sink_ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_PIXEL_FMT,
                               &stream->enc_ctx->pix_fmt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not constrain video sink pixel_formats: %s\n",
                   av_err2str(ret));
            goto end;
        }
        ret = avfilter_init_dict(sink_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not initialize video buffer sink: %s\n",
                   av_err2str(ret));
            goto end;
        }
    } else if (stream->type == AVMEDIA_TYPE_AUDIO) {
        char layout[128];

        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            ret = AVERROR_FILTER_NOT_FOUND;
            goto end;
        }

        if (stream->dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&stream->dec_ctx->ch_layout,
                                      stream->dec_ctx->ch_layout.nb_channels > 0
                                          ? stream->dec_ctx->ch_layout.nb_channels : 2);
        av_channel_layout_describe(&stream->dec_ctx->ch_layout, layout, sizeof(layout));
        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
                 stream->dec_ctx->pkt_timebase.num, stream->dec_ctx->pkt_timebase.den,
                 stream->dec_ctx->sample_rate,
                 av_get_sample_fmt_name(stream->dec_ctx->sample_fmt), layout);

        ret = avfilter_graph_create_filter(&src_ctx, buffersrc, "in", args, NULL, graph);
        if (ret < 0) goto end;
        sink_ctx = avfilter_graph_alloc_filter(graph, buffersink, "out");
        if (!sink_ctx) { ret = AVERROR(ENOMEM); goto end; }

        ret = av_opt_set_array(sink_ctx, "sample_formats", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_SAMPLE_FMT,
                               &stream->enc_ctx->sample_fmt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not constrain audio sink sample_formats: %s\n",
                   av_err2str(ret));
            goto end;
        }

        ret = av_opt_set_array(sink_ctx, "channel_layouts", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_CHLAYOUT,
                               &stream->enc_ctx->ch_layout);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not constrain audio sink channel_layouts: %s\n",
                   av_err2str(ret));
            goto end;
        }
        ret = av_opt_set_array(sink_ctx, "samplerates", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_INT,
                               &stream->enc_ctx->sample_rate);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not constrain audio sink samplerates: %s\n",
                   av_err2str(ret));
            goto end;
        }
        if (stream->enc_ctx->frame_size > 0)
            av_buffersink_set_frame_size(sink_ctx, stream->enc_ctx->frame_size);
        ret = avfilter_init_dict(sink_ctx, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not initialize audio buffer sink: %s\n",
                   av_err2str(ret));
            goto end;
        }
    } else {
        ret = AVERROR(EINVAL);
        goto end;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = src_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = sink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ret = avfilter_graph_parse_ptr(graph, filter_spec, &inputs, &outputs, NULL);
    if (ret < 0) goto end;
    ret = avfilter_graph_config(graph, NULL);
    if (ret < 0) goto end;

    stream->filter_graph = graph;
    stream->buffersrc_ctx = src_ctx;
    stream->buffersink_ctx = sink_ctx;
    graph = NULL;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avfilter_graph_free(&graph);
    return ret;
}

static int append_filter(char *buffer, size_t buffer_size, const char *value)
{
    size_t used = strlen(buffer);
    if (used && used + 1 < buffer_size) {
        buffer[used++] = ',';
        buffer[used] = '\0';
    }
    if (used + strlen(value) + 1 > buffer_size)
        return AVERROR(ENOSPC);
    strcat(buffer, value);
    return 0;
}

static int append_autorotate_filter(const StreamContext *stream, char *buffer, size_t buffer_size)
{
    char spec[64];
    double theta;
    int ret;

    if (!stream->has_display_matrix)
        return 0;
    theta = stream->autorotate_degrees;

    if (fabs(theta - 90.0) < 1.0) {
        snprintf(spec, sizeof(spec), "transpose=%s",
                 stream->display_matrix[3] > 0 ? "cclock_flip" : "clock");
        return append_filter(buffer, buffer_size, spec);
    }
    if (fabs(theta - 180.0) < 1.0) {
        if (stream->display_matrix[0] < 0) {
            ret = append_filter(buffer, buffer_size, "hflip");
            if (ret < 0) return ret;
        }
        if (stream->display_matrix[4] < 0)
            return append_filter(buffer, buffer_size, "vflip");
        return 0;
    }
    if (fabs(theta - 270.0) < 1.0) {
        snprintf(spec, sizeof(spec), "transpose=%s",
                 stream->display_matrix[3] < 0 ? "clock_flip" : "cclock");
        return append_filter(buffer, buffer_size, spec);
    }
    if (fabs(theta) < 1.0 && stream->display_matrix[4] < 0)
        return append_filter(buffer, buffer_size, "vflip");

    if (fabs(theta) >= 1.0)
        av_log(NULL, AV_LOG_WARNING, "Unsupported odd display rotation %.2f degrees; keeping coded orientation\n", theta);
    return 0;
}

static int init_filters(RunnerContext *ctx)
{
    char video_filter[768] = {0};
    char part[256];
    char audio_filter[128];
    int ret;

    ret = append_autorotate_filter(&ctx->video, video_filter, sizeof(video_filter));
    if (ret < 0)
        return ret;

    if (ctx->options.fps > 0.0) {
        snprintf(part, sizeof(part), "fps=fps=%.6f", ctx->options.fps);
        ret = append_filter(video_filter, sizeof(video_filter), part);
        if (ret < 0) return ret;
    }
    snprintf(part, sizeof(part), "scale=%d:%d:flags=bicubic", ctx->video.enc_ctx->width, ctx->video.enc_ctx->height);
    ret = append_filter(video_filter, sizeof(video_filter), part);
    if (ret < 0) return ret;
    ret = append_filter(video_filter, sizeof(video_filter), "format=pix_fmts=yuv420p");
    if (ret < 0) return ret;
    ret = append_filter(video_filter, sizeof(video_filter), "setsar=1");
    if (ret < 0) return ret;

    ret = init_filter(&ctx->video, video_filter);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not initialize video filter: %s\n", video_filter);
        return ret;
    }

    if (ctx->have_audio) {
        snprintf(audio_filter, sizeof(audio_filter), "aresample=%d", ctx->audio.enc_ctx->sample_rate);
        ret = init_filter(&ctx->audio, audio_filter);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not initialize audio filter: %s\n", audio_filter);
            return ret;
        }
    }
    return 0;
}

static int encode_write_frame(RunnerContext *ctx, StreamContext *stream, int flush)
{
    AVFrame *frame = flush ? NULL : stream->filtered_frame;
    int ret;

    av_packet_unref(stream->enc_pkt);
    if (frame && frame->pts != AV_NOPTS_VALUE)
        frame->pts = av_rescale_q(frame->pts, frame->time_base, stream->enc_ctx->time_base);

    ret = avcodec_send_frame(stream->enc_ctx, frame);
    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, stream->enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;

        stream->enc_pkt->stream_index = stream->output_index;
        av_packet_rescale_ts(stream->enc_pkt,
                             stream->enc_ctx->time_base,
                             ctx->ofmt_ctx->streams[stream->output_index]->time_base);
        ret = av_interleaved_write_frame(ctx->ofmt_ctx, stream->enc_pkt);
        if (ret < 0)
            return ret;
    }
    return ret;
}

static int filter_encode_write_frame(RunnerContext *ctx, StreamContext *stream, AVFrame *frame)
{
    int ret;

    ret = av_buffersrc_add_frame_flags(stream->buffersrc_ctx, frame, 0);
    if (ret < 0)
        return ret;

    while (1) {
        ret = av_buffersink_get_frame(stream->buffersink_ctx, stream->filtered_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;

        stream->filtered_frame->time_base = av_buffersink_get_time_base(stream->buffersink_ctx);
        stream->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(ctx, stream, 0);
        av_frame_unref(stream->filtered_frame);
        if (ret < 0)
            return ret;
    }
}

static int process_packet(RunnerContext *ctx, StreamContext *stream, AVPacket *packet)
{
    int ret;

    ret = avcodec_send_packet(stream->dec_ctx, packet);
    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;

        stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
        ret = filter_encode_write_frame(ctx, stream, stream->dec_frame);
        av_frame_unref(stream->dec_frame);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int flush_stream(RunnerContext *ctx, StreamContext *stream)
{
    int ret;

    ret = avcodec_send_packet(stream->dec_ctx, NULL);
    if (ret < 0 && ret != AVERROR_EOF)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            break;
        if (ret < 0)
            return ret;

        stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
        ret = filter_encode_write_frame(ctx, stream, stream->dec_frame);
        av_frame_unref(stream->dec_frame);
        if (ret < 0)
            return ret;
    }

    ret = filter_encode_write_frame(ctx, stream, NULL);
    if (ret < 0)
        return ret;

    return encode_write_frame(ctx, stream, 1);
}

static void report_progress(RunnerContext *ctx, const AVPacket *packet)
{
    AVStream *stream;
    int64_t timestamp;
    double progress;

    if (ctx->duration_us <= 0 || packet->stream_index < 0 ||
        packet->stream_index >= (int)ctx->ifmt_ctx->nb_streams)
        return;

    timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (timestamp == AV_NOPTS_VALUE)
        return;

    stream = ctx->ifmt_ctx->streams[packet->stream_index];
    progress = (double)av_rescale_q(timestamp, stream->time_base, AV_TIME_BASE_Q) /
               (double)ctx->duration_us;
    progress = FFMIN(1.0, FFMAX(0.0, progress));
    if (progress - ctx->last_progress >= 0.005) {
        printf(PROGRESS_PREFIX " %.6f\n", progress);
        fflush(stdout);
        ctx->last_progress = progress;
    }
}

typedef struct PacketMeasure {
    int stream_index;
    int64_t bytes;
    int64_t first_us;
    int64_t last_us;
    int seen_timestamp;
} PacketMeasure;

static void measure_packet(PacketMeasure *measure, const AVPacket *packet, const AVStream *stream)
{
    int64_t timestamp;
    int64_t start_us;
    int64_t end_us;

    if (packet->stream_index != measure->stream_index)
        return;
    if (packet->size > 0)
        measure->bytes += packet->size;

    timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (timestamp == AV_NOPTS_VALUE)
        return;
    start_us = av_rescale_q(timestamp, stream->time_base, AV_TIME_BASE_Q);
    end_us = start_us;
    if (packet->duration > 0)
        end_us += av_rescale_q(packet->duration, stream->time_base, AV_TIME_BASE_Q);
    if (!measure->seen_timestamp) {
        measure->first_us = start_us;
        measure->last_us = end_us;
        measure->seen_timestamp = 1;
    } else {
        measure->first_us = FFMIN(measure->first_us, start_us);
        measure->last_us = FFMAX(measure->last_us, end_us);
    }
}

static double stream_duration_seconds(const AVFormatContext *format, const AVStream *stream,
                                      const PacketMeasure *measure)
{
    if (stream && stream->duration != AV_NOPTS_VALUE && stream->duration > 0)
        return stream->duration * av_q2d(stream->time_base);
    if (measure && measure->seen_timestamp && measure->last_us > measure->first_us)
        return (measure->last_us - measure->first_us) / (double)AV_TIME_BASE;
    if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
        return format->duration / (double)AV_TIME_BASE;
    return NAN;
}

static int64_t measured_kbps(const PacketMeasure *measure, double duration_seconds)
{
    if (!measure || measure->bytes <= 0 || !isfinite(duration_seconds) || duration_seconds <= 0.0)
        return 0;
    return (int64_t)llround((measure->bytes * 8.0) / duration_seconds / 1000.0);
}

static int inspect_media(const RunnerOptions *options)
{
    AVFormatContext *format = NULL;
    AVPacket *packet = NULL;
    FILE *out = NULL;
    int video_index;
    int audio_index;
    PacketMeasure video = {.stream_index = -1};
    PacketMeasure audio = {.stream_index = -1};
    int ret;
    double duration;
    double video_duration;
    double audio_duration;
    int64_t video_kbps;
    int64_t audio_kbps;
    AVStream *video_stream;
    AVStream *audio_stream = NULL;
    const AVPacketSideData *side;
    int32_t matrix[9];
    int has_matrix = 0;
    double rotation = 0.0;
    int display_width;
    int display_height;
    AVRational fps;

    ret = avformat_open_input(&format, options->input_path, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(format, NULL);
    if (ret < 0) goto end;

    video_index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0) { ret = video_index; goto end; }
    audio_index = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
    video.stream_index = video_index;
    audio.stream_index = audio_index;
    video_stream = format->streams[video_index];
    if (audio_index >= 0) audio_stream = format->streams[audio_index];

    packet = av_packet_alloc();
    if (!packet) { ret = AVERROR(ENOMEM); goto end; }
    while ((ret = av_read_frame(format, packet)) >= 0) {
        measure_packet(&video, packet, video_stream);
        if (audio_stream) measure_packet(&audio, packet, audio_stream);
        av_packet_unref(packet);
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret < 0) goto end;

    duration = format->duration > 0 ? format->duration / (double)AV_TIME_BASE : NAN;
    video_duration = stream_duration_seconds(format, video_stream, &video);
    audio_duration = audio_stream ? stream_duration_seconds(format, audio_stream, &audio) : NAN;
    if (!isfinite(duration)) duration = video_duration;
    video_kbps = measured_kbps(&video, video_duration);
    audio_kbps = audio_stream ? measured_kbps(&audio, audio_duration) : 0;

    display_width = video_stream->codecpar->width;
    display_height = video_stream->codecpar->height;
    side = codec_side_data(video_stream->codecpar, AV_PKT_DATA_DISPLAYMATRIX);
    if (side && side->size >= 9 * sizeof(int32_t)) {
        memcpy(matrix, side->data, sizeof(matrix));
        has_matrix = 1;
        rotation = get_display_rotation_degrees(matrix);
        if (fabs(rotation - 90.0) < 1.0 || fabs(rotation - 270.0) < 1.0) {
            int temp = display_width;
            display_width = display_height;
            display_height = temp;
        }
    }
    fps = av_guess_frame_rate(format, video_stream, NULL);

    out = fopen(options->inspect_output_path, "wb");
    if (!out) { ret = AVERROR(errno); goto end; }
    fprintf(out,
            "{\"duration\":%.6f,\"video\":{\"codec\":\"%s\",\"width\":%d,\"height\":%d,"
            "\"displayWidth\":%d,\"displayHeight\":%d,\"rotation\":%.0f,\"hasDisplayMatrix\":%s,"
            "\"fps\":%.6f,\"bitRateKbps\":%lld,\"packetBytes\":%lld},\"audio\":",
            isfinite(duration) ? duration : 0.0,
            avcodec_get_name(video_stream->codecpar->codec_id),
            video_stream->codecpar->width, video_stream->codecpar->height,
            display_width, display_height, rotation, has_matrix ? "true" : "false",
            (fps.num > 0 && fps.den > 0) ? av_q2d(fps) : 0.0,
            (long long)video_kbps, (long long)video.bytes);
    if (audio_stream) {
        fprintf(out,
                "{\"codec\":\"%s\",\"bitRateKbps\":%lld,\"packetBytes\":%lld}",
                avcodec_get_name(audio_stream->codecpar->codec_id),
                (long long)audio_kbps, (long long)audio.bytes);
    } else {
        fputs("null", out);
    }
    fputs("}\n", out);

end:
    if (out) fclose(out);
    av_packet_free(&packet);
    avformat_close_input(&format);
    return ret;
}

static int open_input(RunnerContext *ctx)
{
    int video_index;
    int audio_index;
    int ret;

    ret = avformat_open_input(&ctx->ifmt_ctx, ctx->options.input_path, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open input: %s\n", ctx->options.input_path);
        return ret;
    }

    ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL);
    if (ret < 0)
        return ret;

    ctx->duration_us = ctx->ifmt_ctx->duration;

    video_index = av_find_best_stream(ctx->ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0) {
        av_log(NULL, AV_LOG_ERROR, "No video stream found\n");
        return video_index;
    }

    ret = open_decoder(ctx, video_index, &ctx->video);
    if (ret < 0)
        return ret;
    capture_display_matrix(&ctx->video, ctx->ifmt_ctx->streams[video_index]->codecpar);
    ctx->have_video = 1;

    if (!ctx->options.no_audio) {
        audio_index = av_find_best_stream(ctx->ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
        if (audio_index >= 0) {
            ret = open_decoder(ctx, audio_index, &ctx->audio);
            if (ret < 0)
                return ret;
            ctx->have_audio = 1;
        }
    }

    av_dump_format(ctx->ifmt_ctx, 0, ctx->options.input_path, 0);
    return 0;
}

static int open_output(RunnerContext *ctx)
{
    AVDictionary *mux_options = NULL;
    int ret;

    avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL,
                                   !strcmp(ctx->options.codec, "vp9") ? "webm" : "mp4",
                                   ctx->options.output_path);
    if (!ctx->ofmt_ctx)
        return AVERROR_UNKNOWN;

    ret = setup_video_output(ctx);
    if (ret < 0)
        return ret;
    ret = setup_audio_output(ctx);
    if (ret < 0)
        return ret;

    ret = init_filters(ctx);
    if (ret < 0)
        return ret;

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, ctx->options.output_path, AVIO_FLAG_WRITE);
        if (ret < 0)
            return ret;
    }

    if (ctx->options.faststart && !strcmp(ctx->options.codec, "h264"))
        av_dict_set(&mux_options, "movflags", "+faststart", 0);
    ret = avformat_write_header(ctx->ofmt_ctx, &mux_options);
    av_dict_free(&mux_options);
    if (ret < 0)
        return ret;

    av_dump_format(ctx->ofmt_ctx, 0, ctx->options.output_path, 1);
    return 0;
}

static int transcode(RunnerContext *ctx)
{
    AVPacket *packet = av_packet_alloc();
    int ret = 0;

    if (!packet)
        return AVERROR(ENOMEM);

    printf(PROGRESS_PREFIX " 0.000000\n");
    fflush(stdout);

    while ((ret = av_read_frame(ctx->ifmt_ctx, packet)) >= 0) {
        report_progress(ctx, packet);

        if (packet->stream_index == ctx->video.input_index)
            ret = process_packet(ctx, &ctx->video, packet);
        else if (ctx->have_audio && packet->stream_index == ctx->audio.input_index)
            ret = process_packet(ctx, &ctx->audio, packet);
        else
            ret = 0;

        av_packet_unref(packet);
        if (ret < 0)
            break;
    }

    if (ret == AVERROR_EOF)
        ret = 0;
    if (ret < 0)
        goto end;

    ret = flush_stream(ctx, &ctx->video);
    if (ret < 0)
        goto end;
    if (ctx->have_audio) {
        ret = flush_stream(ctx, &ctx->audio);
        if (ret < 0)
            goto end;
    }

    ret = av_write_trailer(ctx->ofmt_ctx);
    if (ret >= 0) {
        printf(PROGRESS_PREFIX " 1.000000\n");
        fflush(stdout);
    }

end:
    av_packet_free(&packet);
    return ret;
}

static void free_stream(StreamContext *stream)
{
    avcodec_free_context(&stream->dec_ctx);
    avcodec_free_context(&stream->enc_ctx);
    av_frame_free(&stream->dec_frame);
    av_frame_free(&stream->filtered_frame);
    av_packet_free(&stream->enc_pkt);
    avfilter_graph_free(&stream->filter_graph);
    stream->buffersrc_ctx = NULL;
    stream->buffersink_ctx = NULL;
}

static void cleanup(RunnerContext *ctx)
{
    free_stream(&ctx->video);
    free_stream(&ctx->audio);
    avformat_close_input(&ctx->ifmt_ctx);
    if (ctx->ofmt_ctx && !(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ctx->ofmt_ctx->pb);
    avformat_free_context(ctx->ofmt_ctx);
    ctx->ofmt_ctx = NULL;
}

int main(int argc, char **argv)
{
    RunnerContext ctx;
    int parse_result;
    int ret;

    memset(&ctx, 0, sizeof(ctx));
    reset_stream(&ctx.video);
    reset_stream(&ctx.audio);
    av_log_set_level(AV_LOG_INFO);

    parse_result = parse_options(argc, argv, &ctx.options);
    if (parse_result > 0)
        return 0;
    if (parse_result < 0) {
        print_usage(argv[0]);
        return 2;
    }

    av_log(NULL, AV_LOG_INFO, "ffmpeg-wasm-runner %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());

    if (ctx.options.inspect_output_path) {
        ret = inspect_media(&ctx.options);
        if (ret < 0)
            av_log(NULL, AV_LOG_ERROR, "Media inspection failed: %s\n", av_err2str(ret));
        return ret < 0 ? 1 : 0;
    }

    ret = open_input(&ctx);
    if (ret < 0)
        goto end;
    ret = open_output(&ctx);
    if (ret < 0)
        goto end;
    ret = transcode(&ctx);

end:
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "FFmpeg WASM runner failed: %s\n", av_err2str(ret));
    cleanup(&ctx);
    return ret < 0 ? 1 : 0;
}
