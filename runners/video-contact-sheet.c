/*
 * FFmpeg WASM Builder - Video Contact Sheet runner.
 *
 * Seeks to evenly distributed points in the selected video stream, decodes one
 * frame around each point, converts it to RGB24 with libswscale, and assembles
 * the thumbnails into one binary PPM image.  No encoder, muxer, libavfilter,
 * swresample, x264, pthread, or SharedArrayBuffer path is required.
 *
 * The browser app can parse the PPM, draw it to Canvas, add timestamp overlays,
 * and export PNG/JPEG with native browser APIs.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>

#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define RUNNER_VERSION "1.3.0"
#define DEFAULT_COUNT 12
#define DEFAULT_THUMB_SIZE 320
#define MIN_THUMB_SIZE 96
#define MAX_THUMB_SIZE 640

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    const char *metadata_output_path;
    int count;
    int columns;
    int thumb_size;
} RunnerOptions;

typedef struct SheetGeometry {
    int count;
    int columns;
    int rows;
    int rotation;
    int cell_width;
    int cell_height;
    int prescale_width;
    int prescale_height;
    int sheet_width;
    int sheet_height;
} SheetGeometry;

static void print_usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM Video Contact Sheet %s\n"
        "Usage:\n"
        "  %s --input INPUT --output OUTPUT [options]\n\n"
        "Options:\n"
        "  --count N                 Thumbnail count: 12, 24, or 48 (default 12)\n"
        "  --thumb-size PX           Long edge of each thumbnail, %d-%d (default %d)\n"
        "  --columns N               Override grid column count (default 4/6/8)\n"
        "  --metadata-output PATH    Optional JSON metadata/sample-time output\n"
        "  --version                 Print runner/FFmpeg version\n"
        "  --help                    Show this help\n\n"
        "The output image is binary PPM (P6/RGB24). Browser code can convert it\n"
        "to PNG/JPEG using Canvas without linking an image encoder into Wasm.\n",
        RUNNER_VERSION, program, MIN_THUMB_SIZE, MAX_THUMB_SIZE, DEFAULT_THUMB_SIZE);
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

static int valid_count(int count)
{
    return count == 12 || count == 24 || count == 48;
}

static int default_columns_for_count(int count)
{
    if (count == 12) return 4;
    if (count == 24) return 6;
    return 8;
}

static int parse_options(int argc, char **argv, RunnerOptions *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->count = DEFAULT_COUNT;
    options->thumb_size = DEFAULT_THUMB_SIZE;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            print_usage(argv[0]);
            return 1;
        }
        if (!strcmp(arg, "--version")) {
            printf("video-contact-sheet %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
            return 1;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return AVERROR(EINVAL);
        }
        value = argv[++i];

        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--output")) options->output_path = value;
        else if (!strcmp(arg, "--metadata-output")) options->metadata_output_path = value;
        else if (!strcmp(arg, "--count")) {
            if (parse_int(value, 1, 1000, &options->count) < 0 || !valid_count(options->count)) {
                fprintf(stderr, "--count must be 12, 24, or 48.\n");
                return AVERROR(EINVAL);
            }
        } else if (!strcmp(arg, "--columns")) {
            if (parse_int(value, 1, 16, &options->columns) < 0) {
                fprintf(stderr, "--columns must be between 1 and 16.\n");
                return AVERROR(EINVAL);
            }
        } else if (!strcmp(arg, "--thumb-size")) {
            if (parse_int(value, MIN_THUMB_SIZE, MAX_THUMB_SIZE, &options->thumb_size) < 0) {
                fprintf(stderr, "--thumb-size must be between %d and %d.\n",
                        MIN_THUMB_SIZE, MAX_THUMB_SIZE);
                return AVERROR(EINVAL);
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
    }

    if (!options->input_path || !options->output_path) {
        fprintf(stderr, "--input and --output are required.\n");
        return AVERROR(EINVAL);
    }
    if (!valid_count(options->count)) {
        fprintf(stderr, "--count must be 12, 24, or 48.\n");
        return AVERROR(EINVAL);
    }
    if (options->columns == 0)
        options->columns = default_columns_for_count(options->count);
    if (options->columns > options->count) {
        fprintf(stderr, "--columns cannot be greater than --count.\n");
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

    if (!side || side->size < 9 * sizeof(int32_t))
        return 0;
    angle = av_display_rotation_get((const int32_t *)side->data);
    if (!isfinite(angle))
        return 0;

    quarter = (int)llround(angle / 90.0);
    rotation = (quarter * 90) % 360;
    if (rotation < 0) rotation += 360;
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)
        return 0;
    return rotation;
}

static double effective_sample_aspect_ratio(const AVStream *stream)
{
    AVRational sar = stream->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0)
        sar = stream->codecpar->sample_aspect_ratio;
    if (sar.num <= 0 || sar.den <= 0)
        return 1.0;
    return av_q2d(sar);
}

static int compute_geometry(const RunnerOptions *options,
                            const AVStream *stream,
                            SheetGeometry *geometry)
{
    const AVCodecParameters *par = stream->codecpar;
    double sar;
    double encoded_width;
    double encoded_height;
    double display_width;
    double display_height;
    double aspect;
    int rotation;
    int cell_w;
    int cell_h;
    int64_t sheet_w;
    int64_t sheet_h;

    if (!par || par->width <= 0 || par->height <= 0)
        return AVERROR(EINVAL);

    rotation = normalized_quarter_rotation(par);
    sar = effective_sample_aspect_ratio(stream);
    encoded_width = (double)par->width * sar;
    encoded_height = (double)par->height;
    if (rotation == 90 || rotation == 270) {
        display_width = encoded_height;
        display_height = encoded_width;
    } else {
        display_width = encoded_width;
        display_height = encoded_height;
    }
    if (!(display_width > 0.0) || !(display_height > 0.0))
        return AVERROR(EINVAL);

    aspect = display_width / display_height;
    if (aspect >= 1.0) {
        cell_w = options->thumb_size;
        cell_h = (int)llround((double)options->thumb_size / aspect);
    } else {
        cell_h = options->thumb_size;
        cell_w = (int)llround((double)options->thumb_size * aspect);
    }
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;

    memset(geometry, 0, sizeof(*geometry));
    geometry->count = options->count;
    geometry->columns = options->columns;
    geometry->rows = (options->count + options->columns - 1) / options->columns;
    geometry->rotation = rotation;
    geometry->cell_width = cell_w;
    geometry->cell_height = cell_h;
    if (rotation == 90 || rotation == 270) {
        geometry->prescale_width = cell_h;
        geometry->prescale_height = cell_w;
    } else {
        geometry->prescale_width = cell_w;
        geometry->prescale_height = cell_h;
    }

    sheet_w = (int64_t)geometry->columns * geometry->cell_width;
    sheet_h = (int64_t)geometry->rows * geometry->cell_height;
    if (sheet_w <= 0 || sheet_h <= 0 || sheet_w > INT32_MAX || sheet_h > INT32_MAX)
        return AVERROR(EINVAL);
    geometry->sheet_width = (int)sheet_w;
    geometry->sheet_height = (int)sheet_h;
    return 0;
}

static int64_t video_duration_us(const AVFormatContext *format, const AVStream *stream)
{
    if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0)
        return av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q);
    if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
        return format->duration;
    return AV_NOPTS_VALUE;
}

static int64_t stream_start_ts(const AVStream *stream)
{
    return stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
}

static int seek_to_target(AVFormatContext *format, AVStream *stream, int stream_index,
                          int64_t target_ts)
{
    int ret;

    ret = av_seek_frame(format, stream_index, target_ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        ret = avformat_seek_file(format, stream_index, INT64_MIN, target_ts, target_ts,
                                 AVSEEK_FLAG_BACKWARD);
    }
    if (ret >= 0)
        avformat_flush(format);
    return ret;
}

static int frame_reaches_target(const AVFrame *frame, int64_t target_ts)
{
    int64_t pts = frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE)
        return 1;
    return pts >= target_ts;
}

static int remember_frame(AVFrame *candidate, int64_t *candidate_ts, const AVFrame *frame)
{
    int ret;
    av_frame_unref(candidate);
    ret = av_frame_ref(candidate, frame);
    if (ret < 0)
        return ret;
    *candidate_ts = frame->best_effort_timestamp;
    return 0;
}

static int drain_decoder_until_target(AVCodecContext *decoder,
                                      AVFrame *decoded,
                                      AVFrame *candidate,
                                      int64_t *candidate_ts,
                                      int64_t target_ts,
                                      AVFrame *output,
                                      int *found)
{
    int ret;

    while ((ret = avcodec_receive_frame(decoder, decoded)) >= 0) {
        if (frame_reaches_target(decoded, target_ts)) {
            av_frame_unref(output);
            av_frame_move_ref(output, decoded);
            *found = 1;
            return 0;
        }
        ret = remember_frame(candidate, candidate_ts, decoded);
        av_frame_unref(decoded);
        if (ret < 0)
            return ret;
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return 0;
    return ret;
}

static int decode_sample(AVFormatContext *format,
                         AVCodecContext *decoder,
                         AVStream *stream,
                         int stream_index,
                         int64_t target_ts,
                         AVPacket *packet,
                         AVFrame *decoded,
                         AVFrame *candidate,
                         AVFrame *output,
                         int64_t *actual_ts)
{
    int64_t candidate_ts = AV_NOPTS_VALUE;
    int found = 0;
    int ret;

    av_frame_unref(decoded);
    av_frame_unref(candidate);
    av_frame_unref(output);
    av_packet_unref(packet);

    ret = seek_to_target(format, stream, stream_index, target_ts);
    if (ret < 0)
        return ret;
    avcodec_flush_buffers(decoder);

    while ((ret = av_read_frame(format, packet)) >= 0) {
        if (packet->stream_index != stream_index) {
            av_packet_unref(packet);
            continue;
        }

        ret = avcodec_send_packet(decoder, packet);
        if (ret == AVERROR(EAGAIN)) {
            ret = drain_decoder_until_target(decoder, decoded, candidate, &candidate_ts,
                                             target_ts, output, &found);
            if (ret < 0) {
                av_packet_unref(packet);
                return ret;
            }
            if (found) {
                av_packet_unref(packet);
                *actual_ts = output->best_effort_timestamp;
                return 0;
            }
            ret = avcodec_send_packet(decoder, packet);
        }
        av_packet_unref(packet);
        if (ret < 0)
            return ret;

        ret = drain_decoder_until_target(decoder, decoded, candidate, &candidate_ts,
                                         target_ts, output, &found);
        if (ret < 0)
            return ret;
        if (found) {
            *actual_ts = output->best_effort_timestamp;
            return 0;
        }
    }
    if (ret != AVERROR_EOF)
        return ret;

    ret = avcodec_send_packet(decoder, NULL);
    if (ret < 0 && ret != AVERROR_EOF)
        return ret;
    ret = drain_decoder_until_target(decoder, decoded, candidate, &candidate_ts,
                                     target_ts, output, &found);
    if (ret < 0)
        return ret;
    if (found) {
        *actual_ts = output->best_effort_timestamp;
        return 0;
    }

    if (candidate->buf[0]) {
        av_frame_move_ref(output, candidate);
        *actual_ts = candidate_ts;
        return 0;
    }
    return AVERROR_EOF;
}

static void copy_pixel(uint8_t *dst, const uint8_t *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void place_rgb_tile(uint8_t *sheet,
                           const SheetGeometry *geometry,
                           int tile_index,
                           const uint8_t *rgb,
                           int rgb_linesize)
{
    const int col = tile_index % geometry->columns;
    const int row = tile_index / geometry->columns;
    const int x0 = col * geometry->cell_width;
    const int y0 = row * geometry->cell_height;
    const int src_w = geometry->prescale_width;
    const int src_h = geometry->prescale_height;
    int dx;
    int dy;

    for (dy = 0; dy < geometry->cell_height; dy++) {
        for (dx = 0; dx < geometry->cell_width; dx++) {
            int sx;
            int sy;
            const uint8_t *src;
            uint8_t *dst;

            if (geometry->rotation == 90) {
                /* Positive display-matrix rotation: counter-clockwise. */
                sx = src_w - 1 - dy;
                sy = dx;
            } else if (geometry->rotation == 180) {
                sx = src_w - 1 - dx;
                sy = src_h - 1 - dy;
            } else if (geometry->rotation == 270) {
                sx = dy;
                sy = src_h - 1 - dx;
            } else {
                sx = dx;
                sy = dy;
            }

            src = rgb + (size_t)sy * rgb_linesize + (size_t)sx * 3;
            dst = sheet + (((size_t)(y0 + dy) * geometry->sheet_width) + (x0 + dx)) * 3;
            copy_pixel(dst, src);
        }
    }
}

static int frame_to_tile(struct SwsContext **sws,
                         const AVFrame *frame,
                         const SheetGeometry *geometry,
                         uint8_t *rgb,
                         size_t rgb_capacity,
                         uint8_t *sheet,
                         int tile_index)
{
    uint8_t *dst_data[4] = {0};
    int dst_linesize[4] = {0};
    int needed;
    int scaled;

    needed = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                      geometry->prescale_width,
                                      geometry->prescale_height, 1);
    if (needed < 0)
        return needed;
    if ((size_t)needed > rgb_capacity)
        return AVERROR(ENOMEM);

    if (av_image_fill_arrays(dst_data, dst_linesize, rgb, AV_PIX_FMT_RGB24,
                             geometry->prescale_width,
                             geometry->prescale_height, 1) < 0)
        return AVERROR(EINVAL);

    *sws = sws_getCachedContext(*sws,
                                frame->width, frame->height, (enum AVPixelFormat)frame->format,
                                geometry->prescale_width, geometry->prescale_height,
                                AV_PIX_FMT_RGB24,
                                SWS_BILINEAR, NULL, NULL, NULL);
    if (!*sws)
        return AVERROR(ENOMEM);

    scaled = sws_scale(*sws,
                       (const uint8_t * const *)frame->data, frame->linesize,
                       0, frame->height,
                       dst_data, dst_linesize);
    if (scaled != geometry->prescale_height)
        return AVERROR(EINVAL);

    place_rgb_tile(sheet, geometry, tile_index, rgb, dst_linesize[0]);
    return 0;
}

static int write_ppm(const char *path, const SheetGeometry *geometry, const uint8_t *sheet)
{
    FILE *out;
    size_t bytes;

    out = fopen(path, "wb");
    if (!out)
        return AVERROR(errno ? errno : EIO);
    if (fprintf(out, "P6\n%d %d\n255\n", geometry->sheet_width, geometry->sheet_height) < 0) {
        fclose(out);
        return AVERROR(EIO);
    }
    bytes = (size_t)geometry->sheet_width * geometry->sheet_height * 3;
    if (fwrite(sheet, 1, bytes, out) != bytes) {
        fclose(out);
        return AVERROR(EIO);
    }
    if (fclose(out) != 0)
        return AVERROR(EIO);
    return 0;
}

static int write_metadata_json(const char *path,
                               const AVFormatContext *format,
                               const AVStream *stream,
                               const SheetGeometry *geometry,
                               int64_t duration_us,
                               const int64_t *target_us,
                               const int64_t *actual_ts)
{
    FILE *out;
    int i;
    const char *codec_name = avcodec_get_name(stream->codecpar->codec_id);
    int64_t start_ts = stream_start_ts(stream);

    if (!path)
        return 0;
    out = fopen(path, "wb");
    if (!out)
        return AVERROR(errno ? errno : EIO);

    fprintf(out,
            "{\"schemaVersion\":1,\"runnerVersion\":\"%s\","
            "\"format\":\"%s\",\"codec\":\"%s\","
            "\"durationSeconds\":%.6f,\"count\":%d,\"columns\":%d,\"rows\":%d,"
            "\"rotationDegrees\":%d,\"cellWidth\":%d,\"cellHeight\":%d,"
            "\"sheetWidth\":%d,\"sheetHeight\":%d,\"samples\":[",
            RUNNER_VERSION,
            format->iformat && format->iformat->name ? format->iformat->name : "",
            codec_name ? codec_name : "unknown",
            (double)duration_us / AV_TIME_BASE,
            geometry->count, geometry->columns, geometry->rows,
            geometry->rotation, geometry->cell_width, geometry->cell_height,
            geometry->sheet_width, geometry->sheet_height);

    for (i = 0; i < geometry->count; i++) {
        double actual_seconds;
        if (i) fputc(',', out);
        if (actual_ts[i] == AV_NOPTS_VALUE)
            actual_seconds = (double)target_us[i] / AV_TIME_BASE;
        else
            actual_seconds = av_q2d(stream->time_base) * (double)(actual_ts[i] - start_ts);
        if (actual_seconds < 0.0)
            actual_seconds = 0.0;
        fprintf(out,
                "{\"index\":%d,\"targetSeconds\":%.6f,\"actualSeconds\":%.6f}",
                i, (double)target_us[i] / AV_TIME_BASE, actual_seconds);
    }
    fputs("]}\n", out);
    if (fclose(out) != 0)
        return AVERROR(EIO);
    return 0;
}

static void emit_progress(int completed, int count)
{
    double progress = count > 0 ? (double)completed / (double)count : 1.0;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    printf(PROGRESS_PREFIX " %.6f\n", progress);
    fflush(stdout);
}

static int run_contact_sheet(const RunnerOptions *options)
{
    AVFormatContext *format = NULL;
    AVCodecContext *decoder = NULL;
    const AVCodec *codec = NULL;
    AVStream *video_stream = NULL;
    AVPacket *packet = NULL;
    AVFrame *decoded = NULL;
    AVFrame *candidate = NULL;
    AVFrame *sample = NULL;
    struct SwsContext *sws = NULL;
    SheetGeometry geometry;
    uint8_t *sheet = NULL;
    uint8_t *rgb = NULL;
    int64_t *targets_us = NULL;
    int64_t *actual_ts = NULL;
    int64_t duration_us;
    int64_t start_ts;
    size_t sheet_bytes;
    int rgb_bytes;
    int video_index;
    int ret = 0;
    int i;

    ret = avformat_open_input(&format, options->input_path, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open input: %s\n", av_err2str(ret));
        goto end;
    }
    ret = avformat_find_stream_info(format, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not read stream information: %s\n", av_err2str(ret));
        goto end;
    }

    video_index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (video_index < 0 || !codec) {
        ret = video_index < 0 ? video_index : AVERROR_DECODER_NOT_FOUND;
        av_log(NULL, AV_LOG_ERROR, "No supported video stream/decoder was found.\n");
        goto end;
    }
    video_stream = format->streams[video_index];

    duration_us = video_duration_us(format, video_stream);
    if (duration_us == AV_NOPTS_VALUE || duration_us <= 0) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Video duration is unavailable; evenly spaced sampling requires a known duration.\n");
        goto end;
    }

    ret = compute_geometry(options, video_stream, &geometry);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not determine contact-sheet geometry.\n");
        goto end;
    }

    decoder = avcodec_alloc_context3(codec);
    if (!decoder) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_parameters_to_context(decoder, video_stream->codecpar);
    if (ret < 0)
        goto end;
    decoder->thread_count = 1;
    ret = avcodec_open2(decoder, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open %s decoder: %s\n",
               codec->name ? codec->name : "video", av_err2str(ret));
        goto end;
    }

    packet = av_packet_alloc();
    decoded = av_frame_alloc();
    candidate = av_frame_alloc();
    sample = av_frame_alloc();
    targets_us = av_calloc(options->count, sizeof(*targets_us));
    actual_ts = av_malloc_array(options->count, sizeof(*actual_ts));
    if (!packet || !decoded || !candidate || !sample || !targets_us || !actual_ts) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    for (i = 0; i < options->count; i++)
        actual_ts[i] = AV_NOPTS_VALUE;

    if ((size_t)geometry.sheet_width > SIZE_MAX / (size_t)geometry.sheet_height / 3) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    sheet_bytes = (size_t)geometry.sheet_width * geometry.sheet_height * 3;
    sheet = av_mallocz(sheet_bytes);
    if (!sheet) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    rgb_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                         geometry.prescale_width,
                                         geometry.prescale_height, 1);
    if (rgb_bytes < 0) {
        ret = rgb_bytes;
        goto end;
    }
    rgb = av_malloc((size_t)rgb_bytes);
    if (!rgb) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    start_ts = stream_start_ts(video_stream);
    printf("contact-sheet: codec=%s count=%d grid=%dx%d cell=%dx%d sheet=%dx%d rotation=%d duration=%.3f\n",
           codec->name ? codec->name : "unknown",
           geometry.count, geometry.columns, geometry.rows,
           geometry.cell_width, geometry.cell_height,
           geometry.sheet_width, geometry.sheet_height,
           geometry.rotation, (double)duration_us / AV_TIME_BASE);
    fflush(stdout);

    for (i = 0; i < options->count; i++) {
        int64_t target_us;
        int64_t target_ts;

        if (options->count == 1)
            target_us = duration_us / 2;
        else
            target_us = av_rescale(duration_us, i, options->count - 1);
        if (target_us >= duration_us && duration_us > 1000)
            target_us = duration_us - 1000;
        if (target_us < 0)
            target_us = 0;
        targets_us[i] = target_us;
        target_ts = start_ts + av_rescale_q(target_us, AV_TIME_BASE_Q, video_stream->time_base);

        ret = decode_sample(format, decoder, video_stream, video_index, target_ts,
                            packet, decoded, candidate, sample, &actual_ts[i]);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not decode sample %d/%d at %.3f s: %s\n",
                   i + 1, options->count, (double)target_us / AV_TIME_BASE,
                   av_err2str(ret));
            goto end;
        }

        ret = frame_to_tile(&sws, sample, &geometry, rgb, (size_t)rgb_bytes, sheet, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not convert sample %d: %s\n", i + 1, av_err2str(ret));
            goto end;
        }
        av_frame_unref(sample);
        emit_progress(i + 1, options->count);
    }

    ret = write_ppm(options->output_path, &geometry, sheet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write contact sheet: %s\n", av_err2str(ret));
        goto end;
    }
    ret = write_metadata_json(options->metadata_output_path, format, video_stream,
                              &geometry, duration_us, targets_us, actual_ts);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write contact-sheet metadata: %s\n", av_err2str(ret));
        goto end;
    }

    printf("contact-sheet: output=%s bytes=%zu samples=%d\n",
           options->output_path, sheet_bytes, options->count);
    fflush(stdout);
    ret = 0;

end:
    sws_freeContext(sws);
    av_free(rgb);
    av_free(sheet);
    av_free(actual_ts);
    av_free(targets_us);
    av_frame_free(&sample);
    av_frame_free(&candidate);
    av_frame_free(&decoded);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avformat_close_input(&format);
    return ret;
}

int main(int argc, char **argv)
{
    RunnerOptions options;
    int ret;

    av_log_set_level(AV_LOG_WARNING);
    ret = parse_options(argc, argv, &options);
    if (ret == 1)
        return 0;
    if (ret < 0)
        return 2;

    ret = run_contact_sheet(&options);
    return ret < 0 ? 1 : 0;
}
