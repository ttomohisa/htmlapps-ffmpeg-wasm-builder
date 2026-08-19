/*
 * Shared public-libav runner for the specialized animated-image profiles.
 * Compile through video-to-gif.c or video-to-webp.c; do not compile directly.
 * Browser File/Blob inputs are expected to be mounted through Emscripten WORKERFS.
 */
#include <errno.h>
#include <inttypes.h>
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
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>

#ifndef ANIMATION_FORMAT_GIF
#define ANIMATION_FORMAT_GIF 0
#endif
#ifndef ANIMATION_FORMAT_WEBP
#define ANIMATION_FORMAT_WEBP 0
#endif
#if (ANIMATION_FORMAT_GIF + ANIMATION_FORMAT_WEBP) != 1
#error "Exactly one animation output format must be selected"
#endif

#define RUNNER_VERSION "1.4.0"
#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define DEFAULT_FPS 15
#define MAX_DIMENSION 8192

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    double start_seconds;
    double end_seconds;
    int max_width;
    int max_height;
    int fps;
#if ANIMATION_FORMAT_GIF
    int colors;
    const char *dither;
#else
    int quality;
    int compression_level;
    int lossless;
#endif
} RunnerOptions;

typedef struct InputContext {
    AVFormatContext *format;
    AVCodecContext *decoder;
    AVStream *stream;
    int stream_index;
    int64_t origin_ts;
} InputContext;

typedef struct FilterContext {
    AVFilterGraph *graph;
    AVFilterContext *video_src;
    AVFilterContext *palette_src;
    AVFilterContext *sink;
} FilterContext;

typedef struct OutputContext {
    AVFormatContext *format;
    AVCodecContext *encoder;
    AVStream *stream;
    AVPacket *packet;
} OutputContext;

typedef struct Geometry {
    int width;
    int height;
    int rotation;
} Geometry;

typedef int (*FrameConsumer)(AVFrame *frame, void *opaque);

static void print_usage(const char *program)
{
#if ANIMATION_FORMAT_GIF
    fprintf(stderr,
            "Usage: %s --input FILE --output FILE [--start SEC] [--end SEC] "
            "[--max-width PX] [--max-height PX] [--fps N] [--colors 16..256] "
            "[--dither sierra2_4a|floyd_steinberg|sierra2|bayer|heckbert]\n",
            program);
#else
    fprintf(stderr,
            "Usage: %s --input FILE --output FILE [--start SEC] [--end SEC] "
            "[--max-width PX] [--max-height PX] [--fps N] [--quality 0..100] "
            "[--compression-level 0..6] [--lossless 0|1]\n",
            program);
#endif
}

static int parse_int(const char *value, int min_value, int max_value, int *out)
{
    char *end = NULL;
    long parsed;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || !end || *end != '\0' || parsed < min_value || parsed > max_value)
        return AVERROR(EINVAL);
    *out = (int)parsed;
    return 0;
}

static int parse_double(const char *value, double min_value, double max_value, double *out)
{
    char *end = NULL;
    double parsed;
    errno = 0;
    parsed = strtod(value, &end);
    if (errno || !end || *end != '\0' || !isfinite(parsed) || parsed < min_value || parsed > max_value)
        return AVERROR(EINVAL);
    *out = parsed;
    return 0;
}

#if ANIMATION_FORMAT_GIF
static int valid_dither(const char *value)
{
    static const char *const allowed[] = {
        "sierra2_4a", "floyd_steinberg", "sierra2", "bayer", "heckbert"
    };
    size_t i;
    for (i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
        if (!strcmp(value, allowed[i])) return 1;
    return 0;
}
#endif

static int parse_options(int argc, char **argv, RunnerOptions *options)
{
    int i;
    memset(options, 0, sizeof(*options));
    options->end_seconds = -1.0;
    options->fps = DEFAULT_FPS;
#if ANIMATION_FORMAT_GIF
    options->colors = 128;
    options->dither = "sierra2_4a";
#else
    options->quality = 75;
    options->compression_level = 4;
#endif

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            print_usage(argv[0]);
            return 1;
        }
        if (!strcmp(arg, "--version")) {
#if ANIMATION_FORMAT_GIF
            printf("video-to-gif %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
#else
            printf("video-to-webp %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
#endif
            return 1;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return AVERROR(EINVAL);
        }
        value = argv[++i];
        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--output")) options->output_path = value;
        else if (!strcmp(arg, "--start")) {
            if (parse_double(value, 0.0, 86400.0, &options->start_seconds) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--end")) {
            if (parse_double(value, 0.0, 86400.0, &options->end_seconds) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--max-width")) {
            if (parse_int(value, 0, MAX_DIMENSION, &options->max_width) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--max-height")) {
            if (parse_int(value, 0, MAX_DIMENSION, &options->max_height) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--fps")) {
            if (parse_int(value, 1, 60, &options->fps) < 0) return AVERROR(EINVAL);
#if ANIMATION_FORMAT_GIF
        } else if (!strcmp(arg, "--colors")) {
            if (parse_int(value, 16, 256, &options->colors) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--dither")) {
            if (!valid_dither(value)) {
                fprintf(stderr, "Unsupported --dither value: %s\n", value);
                return AVERROR(EINVAL);
            }
            options->dither = value;
#else
        } else if (!strcmp(arg, "--quality")) {
            if (parse_int(value, 0, 100, &options->quality) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--compression-level")) {
            if (parse_int(value, 0, 6, &options->compression_level) < 0) return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--lossless")) {
            if (parse_int(value, 0, 1, &options->lossless) < 0) return AVERROR(EINVAL);
#endif
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
    }

    if (!options->input_path || !options->output_path) {
        fprintf(stderr, "--input and --output are required.\n");
        return AVERROR(EINVAL);
    }
    if (options->end_seconds >= 0.0 && options->end_seconds <= options->start_seconds) {
        fprintf(stderr, "--end must be greater than --start.\n");
        return AVERROR(EINVAL);
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

static int normalized_quarter_rotation(const AVCodecParameters *par)
{
    const AVPacketSideData *side = codec_side_data(par, AV_PKT_DATA_DISPLAYMATRIX);
    double angle;
    int quarter;
    int rotation;
    if (!side || side->size < 9 * sizeof(int32_t)) return 0;
    angle = av_display_rotation_get((const int32_t *)side->data);
    if (!isfinite(angle)) return 0;
    quarter = (int)llround(angle / 90.0);
    rotation = (quarter * 90) % 360;
    if (rotation < 0) rotation += 360;
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) return 0;
    return rotation;
}

static double effective_sar(const AVStream *stream)
{
    AVRational sar = stream->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0) sar = stream->codecpar->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0) return 1.0;
    return av_q2d(sar);
}

static int even_dimension(int value)
{
    if (value < 2) return 2;
    return value & ~1;
}

static int compute_geometry(const RunnerOptions *options, const AVStream *stream, Geometry *geometry)
{
    double sar, display_w, display_h, scale = 1.0;
    int rotation;
    if (!stream || !stream->codecpar || stream->codecpar->width <= 0 || stream->codecpar->height <= 0)
        return AVERROR(EINVAL);
    sar = effective_sar(stream);
    display_w = stream->codecpar->width * sar;
    display_h = stream->codecpar->height;
    rotation = normalized_quarter_rotation(stream->codecpar);
    if (rotation == 90 || rotation == 270) {
        double temp = display_w; display_w = display_h; display_h = temp;
    }
    if (options->max_width > 0 && display_w > options->max_width)
        scale = FFMIN(scale, options->max_width / display_w);
    if (options->max_height > 0 && display_h > options->max_height)
        scale = FFMIN(scale, options->max_height / display_h);
    geometry->width = even_dimension((int)llround(display_w * scale));
    geometry->height = even_dimension((int)llround(display_h * scale));
    geometry->rotation = rotation;
    if (geometry->width > MAX_DIMENSION || geometry->height > MAX_DIMENSION)
        return AVERROR(EINVAL);
    return 0;
}

static int open_input(const char *path, InputContext *input)
{
    const AVCodec *decoder;
    int ret;
    memset(input, 0, sizeof(*input));
    ret = avformat_open_input(&input->format, path, NULL, NULL);
    if (ret < 0) return ret;
    ret = avformat_find_stream_info(input->format, NULL);
    if (ret < 0) return ret;
    input->stream_index = av_find_best_stream(input->format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (input->stream_index < 0) return input->stream_index;
    input->stream = input->format->streams[input->stream_index];
    input->decoder = avcodec_alloc_context3(decoder);
    if (!input->decoder) return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(input->decoder, input->stream->codecpar);
    if (ret < 0) return ret;
    input->decoder->pkt_timebase = input->stream->time_base;
    input->decoder->thread_count = 1;
    input->decoder->thread_type = 0;
    ret = avcodec_open2(input->decoder, decoder, NULL);
    if (ret < 0) return ret;
    input->origin_ts = input->stream->start_time != AV_NOPTS_VALUE ? input->stream->start_time : 0;
    return 0;
}

static void close_input(InputContext *input)
{
    avcodec_free_context(&input->decoder);
    avformat_close_input(&input->format);
    memset(input, 0, sizeof(*input));
}

static void close_filter(FilterContext *filter)
{
    avfilter_graph_free(&filter->graph);
    memset(filter, 0, sizeof(*filter));
}

static int set_sink_pix_fmt(AVFilterContext *sink, enum AVPixelFormat format)
{
    int ret = av_opt_set_array(sink, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_PIXEL_FMT, &format);
    if (ret < 0) return ret;
    return avfilter_init_dict(sink, NULL);
}

static int create_video_source(AVFilterGraph *graph, const char *name,
                               const InputContext *input, AVFilterContext **out)
{
    const AVFilter *buffer = avfilter_get_by_name("buffer");
    AVRational sar = input->stream->sample_aspect_ratio;
    char args[512];
    if (!buffer) return AVERROR_FILTER_NOT_FOUND;
    if (sar.num <= 0 || sar.den <= 0) sar = input->stream->codecpar->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0) sar = (AVRational){1, 1};
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             input->decoder->width, input->decoder->height, input->decoder->pix_fmt,
             input->stream->time_base.num, input->stream->time_base.den,
             sar.num, sar.den);
    return avfilter_graph_create_filter(out, buffer, name, args, NULL, graph);
}

static int bind_single_input_graph(FilterContext *filter, const InputContext *input,
                                   const char *spec, enum AVPixelFormat sink_format)
{
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = NULL, *inputs = NULL;
    int ret;
    memset(filter, 0, sizeof(*filter));
    filter->graph = avfilter_graph_alloc();
    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    if (!filter->graph || !outputs || !inputs || !buffersink) { ret = AVERROR(ENOMEM); goto end; }
    ret = create_video_source(filter->graph, "video_src", input, &filter->video_src);
    if (ret < 0) goto end;
    filter->sink = avfilter_graph_alloc_filter(filter->graph, buffersink, "sink");
    if (!filter->sink) { ret = AVERROR(ENOMEM); goto end; }
    ret = set_sink_pix_fmt(filter->sink, sink_format);
    if (ret < 0) goto end;
    outputs->name = av_strdup("in");
    outputs->filter_ctx = filter->video_src;
    outputs->pad_idx = 0;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = filter->sink;
    inputs->pad_idx = 0;
    if (!outputs->name || !inputs->name) { ret = AVERROR(ENOMEM); goto end; }
    ret = avfilter_graph_parse_ptr(filter->graph, spec, &inputs, &outputs, NULL);
    if (ret < 0) goto end;
    ret = avfilter_graph_config(filter->graph, NULL);
end:
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    if (ret < 0) close_filter(filter);
    return ret;
}

#if ANIMATION_FORMAT_GIF
static int bind_paletteuse_graph(FilterContext *filter, const InputContext *input,
                                 const AVFrame *palette, const char *spec)
{
    const AVFilter *buffer = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *sources = NULL, *palette_source = NULL, *sink_input = NULL;
    char args[256];
    int ret;
    memset(filter, 0, sizeof(*filter));
    filter->graph = avfilter_graph_alloc();
    sources = avfilter_inout_alloc();
    palette_source = avfilter_inout_alloc();
    sink_input = avfilter_inout_alloc();
    if (!filter->graph || !sources || !palette_source || !sink_input || !buffer || !buffersink) {
        ret = AVERROR(ENOMEM); goto end;
    }
    ret = create_video_source(filter->graph, "video_src", input, &filter->video_src);
    if (ret < 0) goto end;
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=1/1",
             palette->width, palette->height, palette->format);
    ret = avfilter_graph_create_filter(&filter->palette_src, buffer, "palette_src", args, NULL, filter->graph);
    if (ret < 0) goto end;
    filter->sink = avfilter_graph_alloc_filter(filter->graph, buffersink, "sink");
    if (!filter->sink) { ret = AVERROR(ENOMEM); goto end; }
    ret = set_sink_pix_fmt(filter->sink, AV_PIX_FMT_PAL8);
    if (ret < 0) goto end;

    sources->name = av_strdup("video");
    sources->filter_ctx = filter->video_src;
    sources->pad_idx = 0;
    sources->next = palette_source;
    palette_source->name = av_strdup("palette");
    palette_source->filter_ctx = filter->palette_src;
    palette_source->pad_idx = 0;
    sink_input->name = av_strdup("out");
    sink_input->filter_ctx = filter->sink;
    sink_input->pad_idx = 0;
    if (!sources->name || !palette_source->name || !sink_input->name) { ret = AVERROR(ENOMEM); goto end; }
    ret = avfilter_graph_parse_ptr(filter->graph, spec, &sink_input, &sources, NULL);
    if (ret < 0) goto end;
    ret = avfilter_graph_config(filter->graph, NULL);
    if (ret < 0) goto end;
    {
        AVFrame *palette_copy = av_frame_clone(palette);
        if (!palette_copy) { ret = AVERROR(ENOMEM); goto end; }
        /* palettegen emits its frame at EOF with an end-of-clip timestamp.
         * paletteuse framesync expects a palette available from t=0. */
        palette_copy->pts = 0;
        palette_copy->duration = 1;
        palette_copy->time_base = (AVRational){1, 25};
        ret = av_buffersrc_add_frame_flags(filter->palette_src, palette_copy, AV_BUFFERSRC_FLAG_KEEP_REF);
        av_frame_free(&palette_copy);
        if (ret < 0) goto end;
    }
    ret = av_buffersrc_add_frame_flags(filter->palette_src, NULL, 0);
end:
    avfilter_inout_free(&sources);
    avfilter_inout_free(&sink_input);
    if (ret < 0) close_filter(filter);
    return ret;
}
#endif

static void rotation_prefix(int rotation, char *buffer, size_t size)
{
    if (rotation == 90) snprintf(buffer, size, "transpose=dir=cclock,");
    else if (rotation == 180) snprintf(buffer, size, "hflip,vflip,");
    else if (rotation == 270) snprintf(buffer, size, "transpose=dir=clock,");
    else buffer[0] = '\0';
}

static void common_filter_spec(const RunnerOptions *options, const Geometry *geometry,
                               char *buffer, size_t size)
{
    char rotate[64];
    rotation_prefix(geometry->rotation, rotate, sizeof(rotate));
    snprintf(buffer, size,
             "fps=fps=%d,%sscale=%d:%d:flags=lanczos,setsar=1,setpts=PTS-STARTPTS",
             options->fps, rotate, geometry->width, geometry->height);
}

static void report_progress(double position, const RunnerOptions *options,
                            double base, double span, double *last)
{
    double total, local, value;
    double end = options->end_seconds;
    if (end < 0.0) end = options->start_seconds + 10.0;
    total = FFMAX(0.001, end - options->start_seconds);
    local = (position - options->start_seconds) / total;
    local = FFMAX(0.0, FFMIN(1.0, local));
    value = base + span * local;
    if (value - *last >= 0.005 || value >= 0.999) {
        printf(PROGRESS_PREFIX " %.6f\n", value);
        fflush(stdout);
        *last = value;
    }
}

static int decode_selected(const RunnerOptions *options, InputContext *input,
                           FrameConsumer consumer, void *opaque,
                           double progress_base, double progress_span)
{
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int64_t start_ts, end_ts = AV_NOPTS_VALUE;
    int ret = 0, stop = 0, reading_done = 0;
    double last_progress = progress_base - 1.0;
    if (!packet || !frame) { ret = AVERROR(ENOMEM); goto end; }
    start_ts = input->origin_ts + av_rescale_q((int64_t)llround(options->start_seconds * AV_TIME_BASE),
                                               AV_TIME_BASE_Q, input->stream->time_base);
    if (options->end_seconds >= 0.0)
        end_ts = input->origin_ts + av_rescale_q((int64_t)llround(options->end_seconds * AV_TIME_BASE),
                                                 AV_TIME_BASE_Q, input->stream->time_base);
    if (options->start_seconds > 0.0) {
        ret = av_seek_frame(input->format, input->stream_index, start_ts, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "Seek failed; decoding from the beginning instead.\n");
            ret = 0;
        } else {
            avcodec_flush_buffers(input->decoder);
        }
    }

    while (!reading_done && !stop) {
        ret = av_read_frame(input->format, packet);
        if (ret < 0) { reading_done = 1; break; }
        if (packet->stream_index != input->stream_index) { av_packet_unref(packet); continue; }
        ret = avcodec_send_packet(input->decoder, packet);
        av_packet_unref(packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) goto end;
        while (1) {
            int64_t ts;
            double seconds;
            ret = avcodec_receive_frame(input->decoder, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) { ret = 0; break; }
            if (ret < 0) goto end;
            ts = frame->best_effort_timestamp;
            if (ts == AV_NOPTS_VALUE) { av_frame_unref(frame); continue; }
            if (ts < start_ts) { av_frame_unref(frame); continue; }
            if (end_ts != AV_NOPTS_VALUE && ts >= end_ts) { stop = 1; av_frame_unref(frame); break; }
            frame->pts = ts;
            frame->time_base = input->stream->time_base;
            seconds = av_q2d(input->stream->time_base) * (double)(ts - input->origin_ts);
            report_progress(seconds, options, progress_base, progress_span, &last_progress);
            ret = consumer(frame, opaque);
            av_frame_unref(frame);
            if (ret < 0) goto end;
        }
    }

    if (!stop) {
        ret = avcodec_send_packet(input->decoder, NULL);
        if (ret < 0 && ret != AVERROR_EOF) goto end;
        while (1) {
            int64_t ts;
            ret = avcodec_receive_frame(input->decoder, frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) { ret = 0; break; }
            if (ret < 0) goto end;
            ts = frame->best_effort_timestamp;
            if (ts != AV_NOPTS_VALUE && ts >= start_ts && (end_ts == AV_NOPTS_VALUE || ts < end_ts)) {
                frame->pts = ts;
                frame->time_base = input->stream->time_base;
                ret = consumer(frame, opaque);
                av_frame_unref(frame);
                if (ret < 0) goto end;
            } else av_frame_unref(frame);
        }
    }
end:
    av_packet_free(&packet);
    av_frame_free(&frame);
    return ret;
}

static int feed_filter(AVFrame *frame, void *opaque)
{
    FilterContext *filter = opaque;
    return av_buffersrc_add_frame_flags(filter->video_src, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
}

#if ANIMATION_FORMAT_GIF
static int generate_palette(const RunnerOptions *options, const Geometry *geometry, AVFrame **palette_out)
{
    InputContext input;
    FilterContext filter;
    AVFrame *palette = NULL;
    char common[512], spec[768];
    int ret;
    memset(&input, 0, sizeof(input)); memset(&filter, 0, sizeof(filter));
    *palette_out = NULL;
    ret = open_input(options->input_path, &input); if (ret < 0) goto end;
    common_filter_spec(options, geometry, common, sizeof(common));
    snprintf(spec, sizeof(spec), "%s,palettegen=max_colors=%d[out]", common, options->colors);
    ret = bind_single_input_graph(&filter, &input, spec, AV_PIX_FMT_BGRA); if (ret < 0) goto end;
    ret = decode_selected(options, &input, feed_filter, &filter, 0.0, 0.45); if (ret < 0) goto end;
    ret = av_buffersrc_add_frame_flags(filter.video_src, NULL, 0); if (ret < 0) goto end;
    palette = av_frame_alloc(); if (!palette) { ret = AVERROR(ENOMEM); goto end; }
    ret = av_buffersink_get_frame(filter.sink, palette);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        ret = AVERROR_INVALIDDATA;
        goto end;
    }
    if (ret < 0) goto end;
    *palette_out = palette; palette = NULL;
    ret = 0;
end:
    av_frame_free(&palette);
    close_filter(&filter);
    close_input(&input);
    return ret;
}
#endif

static int open_output(const RunnerOptions *options, const Geometry *geometry, OutputContext *output)
{
    const AVCodec *encoder;
    AVDictionary *codec_options = NULL;
    int ret;
    memset(output, 0, sizeof(*output));
#if ANIMATION_FORMAT_GIF
    avformat_alloc_output_context2(&output->format, NULL, "gif", options->output_path);
    encoder = avcodec_find_encoder(AV_CODEC_ID_GIF);
#else
    avformat_alloc_output_context2(&output->format, NULL, "webp", options->output_path);
    encoder = avcodec_find_encoder_by_name("libwebp_anim");
#endif
    if (!output->format || !encoder) return AVERROR_ENCODER_NOT_FOUND;
    output->stream = avformat_new_stream(output->format, NULL);
    if (!output->stream) return AVERROR(ENOMEM);
    output->encoder = avcodec_alloc_context3(encoder);
    if (!output->encoder) return AVERROR(ENOMEM);
    output->encoder->width = geometry->width;
    output->encoder->height = geometry->height;
    output->encoder->time_base = (AVRational){1, options->fps};
    output->encoder->framerate = (AVRational){options->fps, 1};
    output->encoder->thread_count = 1;
    output->encoder->thread_type = 0;
#if ANIMATION_FORMAT_GIF
    output->encoder->pix_fmt = AV_PIX_FMT_PAL8;
#else
    output->encoder->pix_fmt = options->lossless ? AV_PIX_FMT_BGRA : AV_PIX_FMT_YUV420P;
    av_dict_set_int(&codec_options, "lossless", options->lossless, 0);
    av_dict_set_int(&codec_options, "quality", options->quality, 0);
    av_dict_set_int(&codec_options, "compression_level", options->compression_level, 0);
#endif
    if (output->format->oformat->flags & AVFMT_GLOBALHEADER)
        output->encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ret = avcodec_open2(output->encoder, encoder, &codec_options);
    av_dict_free(&codec_options);
    if (ret < 0) return ret;
    ret = avcodec_parameters_from_context(output->stream->codecpar, output->encoder);
    if (ret < 0) return ret;
    output->stream->time_base = output->encoder->time_base;
    if (!(output->format->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output->format->pb, options->output_path, AVIO_FLAG_WRITE);
        if (ret < 0) return ret;
    }
    if (output->format->priv_data) {
        ret = av_opt_set_int(output->format->priv_data, "loop", 0, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) return ret;
    }
    ret = avformat_write_header(output->format, NULL);
    if (ret < 0) return ret;
    output->packet = av_packet_alloc();
    if (!output->packet) return AVERROR(ENOMEM);
    return 0;
}

static int encode_available(OutputContext *output, AVFrame *frame)
{
    int ret;
    if (frame) {
        AVRational source_tb = frame->time_base.num > 0 && frame->time_base.den > 0
            ? frame->time_base : output->encoder->time_base;
        if (frame->pts != AV_NOPTS_VALUE)
            frame->pts = av_rescale_q(frame->pts, source_tb, output->encoder->time_base);
        if (frame->duration > 0)
            frame->duration = av_rescale_q(frame->duration, source_tb, output->encoder->time_base);
        if (frame->duration <= 0) frame->duration = 1;
        frame->time_base = output->encoder->time_base;
        frame->pict_type = AV_PICTURE_TYPE_NONE;
    }
    ret = avcodec_send_frame(output->encoder, frame);
    if (ret < 0) return ret;
    while (1) {
        ret = avcodec_receive_packet(output->encoder, output->packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
        if (ret < 0) return ret;
        output->packet->stream_index = output->stream->index;
        av_packet_rescale_ts(output->packet, output->encoder->time_base, output->stream->time_base);
        ret = av_interleaved_write_frame(output->format, output->packet);
        av_packet_unref(output->packet);
        if (ret < 0) return ret;
    }
}

static void close_output(OutputContext *output, int write_trailer)
{
    if (output->format && write_trailer) av_write_trailer(output->format);
    av_packet_free(&output->packet);
    avcodec_free_context(&output->encoder);
    if (output->format) {
        if (!(output->format->oformat->flags & AVFMT_NOFILE) && output->format->pb)
            avio_closep(&output->format->pb);
        avformat_free_context(output->format);
    }
    memset(output, 0, sizeof(*output));
}

typedef struct EncodeFilterState {
    FilterContext *filter;
    OutputContext *output;
    AVFrame *filtered;
} EncodeFilterState;

static int drain_encoded_filter(EncodeFilterState *state)
{
    int ret;
    while (1) {
        ret = av_buffersink_get_frame(state->filter->sink, state->filtered);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
        if (ret < 0) return ret;
        state->filtered->time_base = av_buffersink_get_time_base(state->filter->sink);
        ret = encode_available(state->output, state->filtered);
        av_frame_unref(state->filtered);
        if (ret < 0) return ret;
    }
}

static int filter_and_encode(AVFrame *frame, void *opaque)
{
    EncodeFilterState *state = opaque;
    int ret = av_buffersrc_add_frame_flags(state->filter->video_src, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) return ret;
    return drain_encoded_filter(state);
}

static int encode_animation(const RunnerOptions *options, const Geometry *geometry
#if ANIMATION_FORMAT_GIF
                            , const AVFrame *palette
#endif
                            )
{
    InputContext input;
    FilterContext filter;
    OutputContext output;
    EncodeFilterState state;
    AVFrame *filtered = NULL;
    char common[512], spec[1024];
    int ret, trailer_ok = 0;
    memset(&input, 0, sizeof(input)); memset(&filter, 0, sizeof(filter)); memset(&output, 0, sizeof(output));
    ret = open_input(options->input_path, &input); if (ret < 0) goto end;
    common_filter_spec(options, geometry, common, sizeof(common));
#if ANIMATION_FORMAT_GIF
    snprintf(spec, sizeof(spec), "[video]%s[v];[v][palette]paletteuse=dither=%s[out]", common, options->dither);
    ret = bind_paletteuse_graph(&filter, &input, palette, spec); if (ret < 0) goto end;
#else
    {
        enum AVPixelFormat format = options->lossless ? AV_PIX_FMT_BGRA : AV_PIX_FMT_YUV420P;
        snprintf(spec, sizeof(spec), "%s,format=pix_fmts=%s[out]", common,
                 options->lossless ? "bgra" : "yuv420p");
        ret = bind_single_input_graph(&filter, &input, spec, format); if (ret < 0) goto end;
    }
#endif
    ret = open_output(options, geometry, &output); if (ret < 0) goto end;
    filtered = av_frame_alloc(); if (!filtered) { ret = AVERROR(ENOMEM); goto end; }
    state.filter = &filter; state.output = &output; state.filtered = filtered;
#if ANIMATION_FORMAT_GIF
    ret = decode_selected(options, &input, filter_and_encode, &state, 0.45, 0.54);
#else
    ret = decode_selected(options, &input, filter_and_encode, &state, 0.0, 0.99);
#endif
    if (ret < 0) goto end;
    ret = av_buffersrc_add_frame_flags(filter.video_src, NULL, 0); if (ret < 0) goto end;
    ret = drain_encoded_filter(&state); if (ret < 0) goto end;
    ret = encode_available(&output, NULL); if (ret < 0) goto end;
    trailer_ok = 1;
    printf(PROGRESS_PREFIX " 1.000000\n");
    fflush(stdout);
end:
    av_frame_free(&filtered);
    close_output(&output, trailer_ok);
    close_filter(&filter);
    close_input(&input);
    return ret;
}

int main(int argc, char **argv)
{
    RunnerOptions options;
    InputContext probe;
    Geometry geometry;
    int ret;
#if ANIMATION_FORMAT_GIF
    AVFrame *palette = NULL;
#endif
    av_log_set_level(AV_LOG_WARNING);
    ret = parse_options(argc, argv, &options);
    if (ret == 1) return 0;
    if (ret < 0) return 2;

    memset(&probe, 0, sizeof(probe));
    ret = open_input(options.input_path, &probe);
    if (ret < 0) goto fail;
    ret = compute_geometry(&options, probe.stream, &geometry);
    if (ret < 0) { close_input(&probe); goto fail; }
#if ANIMATION_FORMAT_GIF
    printf("video-to-gif: size=%dx%d fps=%d colors=%d dither=%s rotation=%d\n",
           geometry.width, geometry.height, options.fps, options.colors, options.dither, geometry.rotation);
#else
    printf("video-to-webp: size=%dx%d fps=%d quality=%d lossless=%d compression=%d rotation=%d\n",
           geometry.width, geometry.height, options.fps, options.quality, options.lossless,
           options.compression_level, geometry.rotation);
#endif
    close_input(&probe);

#if ANIMATION_FORMAT_GIF
    ret = generate_palette(&options, &geometry, &palette);
    if (ret < 0) goto fail;
    ret = encode_animation(&options, &geometry, palette);
    av_frame_free(&palette);
#else
    ret = encode_animation(&options, &geometry);
#endif
    if (ret < 0) goto fail;
    return 0;
fail:
#if ANIMATION_FORMAT_GIF
    av_frame_free(&palette);
#endif
    fprintf(stderr, "Animation runner failed: %s\n", av_err2str(ret));
    return 1;
}
