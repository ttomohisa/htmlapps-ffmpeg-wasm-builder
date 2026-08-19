/*
 * FFmpeg WASM Builder - Media Inspector runner.
 *
 * Reads container/stream metadata through FFmpeg public libavformat/libavcodec
 * APIs and writes a structured JSON report. No decoder, encoder, filter,
 * swscale, or swresample stage is linked into this profile.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/codec_desc.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/error.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>

#define RUNNER_VERSION "1.2.0"
#define REPORT_SCHEMA_VERSION 1

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
} RunnerOptions;

static void print_usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM Media Inspector %s\n"
        "Usage:\n"
        "  %s --input INPUT --output REPORT.json\n\n"
        "Options:\n"
        "  --input PATH          Media file to inspect\n"
        "  --output PATH         JSON report path\n"
        "  --version             Print runner/FFmpeg version\n"
        "  --help                Show this help\n",
        RUNNER_VERSION, program);
}

static int parse_options(int argc, char **argv, RunnerOptions *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            print_usage(argv[0]);
            return 1;
        }
        if (!strcmp(arg, "--version")) {
            printf("media-inspector %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
            return 1;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return AVERROR(EINVAL);
        }
        value = argv[++i];
        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--output")) options->output_path = value;
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
    }

    if (!options->input_path || !options->output_path) {
        fprintf(stderr, "--input and --output are required.\n");
        return AVERROR(EINVAL);
    }
    return 0;
}

static void json_string(FILE *out, const char *value)
{
    const unsigned char *p;

    if (!value) {
        fputs("null", out);
        return;
    }

    fputc('"', out);
    for (p = (const unsigned char *)value; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out); break;
        case '\f': fputs("\\f", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:
            if (*p < 0x20)
                fprintf(out, "\\u%04x", (unsigned)*p);
            else
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void json_dictionary(FILE *out, const AVDictionary *dict)
{
    const AVDictionaryEntry *entry = NULL;
    int first = 1;

    fputc('{', out);
    while ((entry = av_dict_iterate(dict, entry))) {
        if (!first) fputc(',', out);
        json_string(out, entry->key);
        fputc(':', out);
        json_string(out, entry->value);
        first = 0;
    }
    fputc('}', out);
}

static void json_rational(FILE *out, AVRational value)
{
    if (value.num <= 0 || value.den <= 0) {
        fputs("null", out);
        return;
    }
    fprintf(out, "{\"num\":%d,\"den\":%d,\"value\":%.9f}",
            value.num, value.den, av_q2d(value));
}

static void json_seconds_from_us(FILE *out, int64_t value)
{
    if (value == AV_NOPTS_VALUE || value < 0)
        fputs("null", out);
    else
        fprintf(out, "%.6f", (double)value / AV_TIME_BASE);
}

static void json_stream_seconds(FILE *out, int64_t value, AVRational time_base)
{
    if (value == AV_NOPTS_VALUE || value < 0 || time_base.num <= 0 || time_base.den <= 0)
        fputs("null", out);
    else
        fprintf(out, "%.6f", value * av_q2d(time_base));
}

static void json_i64_or_null(FILE *out, int64_t value)
{
    if (value <= 0)
        fputs("null", out);
    else
        fprintf(out, "%" PRId64, value);
}

static const char *field_order_name(enum AVFieldOrder order)
{
    switch (order) {
    case AV_FIELD_PROGRESSIVE: return "progressive";
    case AV_FIELD_TT: return "tt";
    case AV_FIELD_BB: return "bb";
    case AV_FIELD_TB: return "tb";
    case AV_FIELD_BT: return "bt";
    default: return NULL;
    }
}

static const AVPacketSideData *codec_side_data(const AVCodecParameters *par,
                                                enum AVPacketSideDataType type)
{
    if (!par || !par->coded_side_data || par->nb_coded_side_data <= 0)
        return NULL;
    return av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, type);
}

static double normalized_rotation(const AVCodecParameters *par, int *present)
{
    const AVPacketSideData *side = codec_side_data(par, AV_PKT_DATA_DISPLAYMATRIX);
    double value;

    *present = 0;
    if (!side || side->size < 9 * sizeof(int32_t))
        return 0.0;

    value = av_display_rotation_get((const int32_t *)side->data);
    if (!isfinite(value))
        return 0.0;

    *present = 1;
    value = round(value);
    if (value == -0.0) value = 0.0;
    return value;
}

static void json_mastering_display(FILE *out, const AVCodecParameters *par)
{
    const AVPacketSideData *side = codec_side_data(par, AV_PKT_DATA_MASTERING_DISPLAY_METADATA);
    const AVMasteringDisplayMetadata *meta;

    if (!side || side->size < sizeof(AVMasteringDisplayMetadata)) {
        fputs("null", out);
        return;
    }

    meta = (const AVMasteringDisplayMetadata *)side->data;
    fputs("{\"hasPrimaries\":", out);
    fputs(meta->has_primaries ? "true" : "false", out);
    fputs(",\"hasLuminance\":", out);
    fputs(meta->has_luminance ? "true" : "false", out);

    fputs(",\"displayPrimaries\":", out);
    if (meta->has_primaries) {
        fprintf(out,
            "{\"r\":{\"x\":%.7f,\"y\":%.7f},"
            "\"g\":{\"x\":%.7f,\"y\":%.7f},"
            "\"b\":{\"x\":%.7f,\"y\":%.7f},"
            "\"whitePoint\":{\"x\":%.7f,\"y\":%.7f}}",
            av_q2d(meta->display_primaries[0][0]), av_q2d(meta->display_primaries[0][1]),
            av_q2d(meta->display_primaries[1][0]), av_q2d(meta->display_primaries[1][1]),
            av_q2d(meta->display_primaries[2][0]), av_q2d(meta->display_primaries[2][1]),
            av_q2d(meta->white_point[0]), av_q2d(meta->white_point[1]));
    } else {
        fputs("null", out);
    }

    fputs(",\"minLuminance\":", out);
    if (meta->has_luminance) fprintf(out, "%.6f", av_q2d(meta->min_luminance));
    else fputs("null", out);

    fputs(",\"maxLuminance\":", out);
    if (meta->has_luminance) fprintf(out, "%.6f", av_q2d(meta->max_luminance));
    else fputs("null", out);

    fputc('}', out);
}

static void json_content_light(FILE *out, const AVCodecParameters *par)
{
    const AVPacketSideData *side = codec_side_data(par, AV_PKT_DATA_CONTENT_LIGHT_LEVEL);
    const AVContentLightMetadata *meta;

    if (!side || side->size < sizeof(AVContentLightMetadata)) {
        fputs("null", out);
        return;
    }

    meta = (const AVContentLightMetadata *)side->data;
    fprintf(out, "{\"maxCLL\":%u,\"maxFALL\":%u}", meta->MaxCLL, meta->MaxFALL);
}

static int stream_is_hdr(const AVCodecParameters *par)
{
    if (!par) return 0;
    if (par->color_trc == AVCOL_TRC_SMPTE2084 || par->color_trc == AVCOL_TRC_ARIB_STD_B67)
        return 1;
    if (codec_side_data(par, AV_PKT_DATA_MASTERING_DISPLAY_METADATA)) return 1;
    if (codec_side_data(par, AV_PKT_DATA_CONTENT_LIGHT_LEVEL)) return 1;
    if (codec_side_data(par, AV_PKT_DATA_DOVI_CONF)) return 1;
    if (codec_side_data(par, AV_PKT_DATA_DYNAMIC_HDR10_PLUS)) return 1;
    if (codec_side_data(par, AV_PKT_DATA_DYNAMIC_HDR_SMPTE_2094_APP5)) return 1;
    return 0;
}

static const char *hdr_classification(const AVCodecParameters *par)
{
    if (codec_side_data(par, AV_PKT_DATA_DOVI_CONF)) return "dolby-vision";
    if (codec_side_data(par, AV_PKT_DATA_DYNAMIC_HDR10_PLUS)) return "hdr10-plus";
    if (par->color_trc == AVCOL_TRC_ARIB_STD_B67) return "hlg";
    if (par->color_trc == AVCOL_TRC_SMPTE2084) return "pq";
    if (stream_is_hdr(par)) return "hdr";
    return "sdr-or-unknown";
}

static void json_disposition(FILE *out, int disposition)
{
    fprintf(out,
        "{\"default\":%s,\"forced\":%s,\"hearingImpaired\":%s,"
        "\"visualImpaired\":%s,\"attachedPicture\":%s,\"stillImage\":%s}",
        (disposition & AV_DISPOSITION_DEFAULT) ? "true" : "false",
        (disposition & AV_DISPOSITION_FORCED) ? "true" : "false",
        (disposition & AV_DISPOSITION_HEARING_IMPAIRED) ? "true" : "false",
        (disposition & AV_DISPOSITION_VISUAL_IMPAIRED) ? "true" : "false",
        (disposition & AV_DISPOSITION_ATTACHED_PIC) ? "true" : "false",
        (disposition & AV_DISPOSITION_STILL_IMAGE) ? "true" : "false");
}

static void json_codec_tag(FILE *out, uint32_t tag)
{
    char text[32];

    if (!tag) {
        fputs("null", out);
        return;
    }
    av_fourcc_make_string(text, tag);
    json_string(out, text);
}

static void json_video(FILE *out, AVFormatContext *format, AVStream *stream)
{
    const AVCodecParameters *par = stream->codecpar;
    AVRational sar = stream->sample_aspect_ratio;
    AVRational guessed = av_guess_frame_rate(format, stream, NULL);
    const char *pixel_format = NULL;
    const char *range = NULL;
    const char *primaries = NULL;
    const char *transfer = NULL;
    const char *space = NULL;
    const char *chroma = NULL;
    const char *field = field_order_name(par->field_order);
    int rotation_present = 0;
    double rotation = normalized_rotation(par, &rotation_present);

    if (sar.num <= 0 || sar.den <= 0)
        sar = par->sample_aspect_ratio;
    if (par->format >= 0)
        pixel_format = av_get_pix_fmt_name((enum AVPixelFormat)par->format);
    if (par->color_range != AVCOL_RANGE_UNSPECIFIED) range = av_color_range_name(par->color_range);
    if (par->color_primaries != AVCOL_PRI_UNSPECIFIED) primaries = av_color_primaries_name(par->color_primaries);
    if (par->color_trc != AVCOL_TRC_UNSPECIFIED) transfer = av_color_transfer_name(par->color_trc);
    if (par->color_space != AVCOL_SPC_UNSPECIFIED) space = av_color_space_name(par->color_space);
    if (par->chroma_location != AVCHROMA_LOC_UNSPECIFIED) chroma = av_chroma_location_name(par->chroma_location);

    fprintf(out, "{\"width\":%d,\"height\":%d,\"pixelFormat\":", par->width, par->height);
    json_string(out, pixel_format);

    fputs(",\"frameRate\":", out);
    json_rational(out, guessed);
    fputs(",\"avgFrameRate\":", out);
    json_rational(out, stream->avg_frame_rate);
    fputs(",\"realFrameRate\":", out);
    json_rational(out, stream->r_frame_rate);
    fputs(",\"sampleAspectRatio\":", out);
    json_rational(out, sar);

    fputs(",\"fieldOrder\":", out);
    json_string(out, field);
    fputs(",\"colorRange\":", out);
    json_string(out, range);
    fputs(",\"colorPrimaries\":", out);
    json_string(out, primaries);
    fputs(",\"colorTransfer\":", out);
    json_string(out, transfer);
    fputs(",\"colorSpace\":", out);
    json_string(out, space);
    fputs(",\"chromaLocation\":", out);
    json_string(out, chroma);

    fputs(",\"rotationDegrees\":", out);
    if (rotation_present) fprintf(out, "%.0f", rotation);
    else fputs("null", out);

    fputs(",\"hdr\":{\"isHdr\":", out);
    fputs(stream_is_hdr(par) ? "true" : "false", out);
    fputs(",\"classification\":", out);
    json_string(out, hdr_classification(par));
    fputs(",\"masteringDisplay\":", out);
    json_mastering_display(out, par);
    fputs(",\"contentLight\":", out);
    json_content_light(out, par);
    fputs(",\"dolbyVision\":", out);
    fputs(codec_side_data(par, AV_PKT_DATA_DOVI_CONF) ? "true" : "false", out);
    fputs(",\"dynamicHdr10Plus\":", out);
    fputs(codec_side_data(par, AV_PKT_DATA_DYNAMIC_HDR10_PLUS) ? "true" : "false", out);
    fputs(",\"smpte2094App5\":", out);
    fputs(codec_side_data(par, AV_PKT_DATA_DYNAMIC_HDR_SMPTE_2094_APP5) ? "true" : "false", out);
    fputs("}}", out);
}

static void json_audio(FILE *out, const AVCodecParameters *par)
{
    char reported_layout[256];
    char display_layout[256];
    const char *sample_format = NULL;
    int reported_layout_ok = 0;
    int display_layout_ok = 0;
    int layout_inferred = 0;

    reported_layout[0] = '\0';
    display_layout[0] = '\0';

    if (par->ch_layout.nb_channels > 0 &&
        av_channel_layout_describe(&par->ch_layout, reported_layout, sizeof(reported_layout)) >= 0) {
        reported_layout_ok = 1;
        snprintf(display_layout, sizeof(display_layout), "%s", reported_layout);
        display_layout_ok = 1;
    }

    /*
     * Some containers expose only a channel count when no explicit channel
     * layout is stored. With decoders intentionally disabled, FFmpeg can then
     * describe a two-channel stream as "2 channels" instead of "stereo".
     * For that common case, use FFmpeg's canonical default layout as the
     * UI-friendly value while preserving the raw description and marking the
     * fallback as inferred. No audio frames are decoded.
     */
    if (par->ch_layout.nb_channels == 2 &&
        (par->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC || !reported_layout_ok)) {
        AVChannelLayout default_layout = { 0 };
        char inferred_layout[256];

        inferred_layout[0] = '\0';
        av_channel_layout_default(&default_layout, par->ch_layout.nb_channels);
        if (av_channel_layout_describe(&default_layout, inferred_layout, sizeof(inferred_layout)) >= 0 &&
            inferred_layout[0] != '\0') {
            snprintf(display_layout, sizeof(display_layout), "%s", inferred_layout);
            display_layout_ok = 1;
            layout_inferred = 1;
        }
        av_channel_layout_uninit(&default_layout);
    }

    if (par->format >= 0)
        sample_format = av_get_sample_fmt_name((enum AVSampleFormat)par->format);

    fprintf(out, "{\"sampleRate\":%d,\"channels\":%d,\"channelLayout\":",
            par->sample_rate, par->ch_layout.nb_channels);
    json_string(out, display_layout_ok ? display_layout : NULL);
    fputs(",\"channelLayoutReported\":", out);
    json_string(out, reported_layout_ok ? reported_layout : NULL);
    fprintf(out, ",\"channelLayoutInferred\":%s", layout_inferred ? "true" : "false");
    fputs(",\"sampleFormat\":", out);
    json_string(out, sample_format);
    fputs(",\"bitRate\":", out);
    json_i64_or_null(out, par->bit_rate);
    fprintf(out,
            ",\"bitsPerCodedSample\":%d,\"bitsPerRawSample\":%d,"
            "\"initialPadding\":%d,\"trailingPadding\":%d}",
            par->bits_per_coded_sample, par->bits_per_raw_sample,
            par->initial_padding, par->trailing_padding);
}

static void json_subtitle(FILE *out, const AVCodecDescriptor *descriptor)
{
    int text_based = descriptor && (descriptor->props & AV_CODEC_PROP_TEXT_SUB);
    int bitmap_based = descriptor && (descriptor->props & AV_CODEC_PROP_BITMAP_SUB);

    fprintf(out, "{\"textBased\":%s,\"bitmapBased\":%s}",
            text_based ? "true" : "false",
            bitmap_based ? "true" : "false");
}

static void json_stream(FILE *out, AVFormatContext *format, AVStream *stream)
{
    const AVCodecParameters *par = stream->codecpar;
    const AVCodecDescriptor *descriptor = avcodec_descriptor_get(par->codec_id);
    const char *media_type = av_get_media_type_string(par->codec_type);
    const char *codec_name = avcodec_get_name(par->codec_id);
    const char *profile_name = NULL;

    if (par->profile != AV_PROFILE_UNKNOWN)
        profile_name = avcodec_profile_name(par->codec_id, par->profile);

    fprintf(out, "{\"index\":%d,\"id\":%d,\"type\":", stream->index, stream->id);
    json_string(out, media_type ? media_type : "unknown");

    fputs(",\"codec\":{\"name\":", out);
    json_string(out, codec_name);
    fputs(",\"longName\":", out);
    json_string(out, descriptor ? descriptor->long_name : NULL);
    fputs(",\"tag\":", out);
    json_codec_tag(out, par->codec_tag);
    fprintf(out, ",\"tagHex\":\"0x%08" PRIx32 "\"", par->codec_tag);
    fputs(",\"profile\":", out);
    json_string(out, profile_name);
    fputs(",\"profileId\":", out);
    if (par->profile == AV_PROFILE_UNKNOWN) fputs("null", out);
    else fprintf(out, "%d", par->profile);
    fputs(",\"level\":", out);
    if (par->level == AV_LEVEL_UNKNOWN) fputs("null", out);
    else fprintf(out, "%d", par->level);
    fputs(",\"bitRate\":", out);
    json_i64_or_null(out, par->bit_rate);
    fprintf(out, ",\"extradataSize\":%d}", par->extradata_size);

    fputs(",\"timeBase\":", out);
    json_rational(out, stream->time_base);
    fputs(",\"startTime\":", out);
    json_stream_seconds(out, stream->start_time, stream->time_base);
    fputs(",\"duration\":", out);
    json_stream_seconds(out, stream->duration, stream->time_base);
    fputs(",\"frameCount\":", out);
    if (stream->nb_frames > 0) fprintf(out, "%" PRId64, stream->nb_frames);
    else fputs("null", out);

    fputs(",\"disposition\":", out);
    json_disposition(out, stream->disposition);
    fputs(",\"metadata\":", out);
    json_dictionary(out, stream->metadata);

    if (par->codec_type == AVMEDIA_TYPE_VIDEO) {
        fputs(",\"video\":", out);
        json_video(out, format, stream);
    } else if (par->codec_type == AVMEDIA_TYPE_AUDIO) {
        fputs(",\"audio\":", out);
        json_audio(out, par);
    } else if (par->codec_type == AVMEDIA_TYPE_SUBTITLE) {
        fputs(",\"subtitle\":", out);
        json_subtitle(out, descriptor);
    }

    fputc('}', out);
}

static void json_chapters(FILE *out, const AVFormatContext *format)
{
    unsigned int i;

    fputc('[', out);
    for (i = 0; i < format->nb_chapters; i++) {
        const AVChapter *chapter = format->chapters[i];
        if (i) fputc(',', out);
        fprintf(out, "{\"id\":%" PRId64 ",\"start\":", chapter->id);
        json_stream_seconds(out, chapter->start, chapter->time_base);
        fputs(",\"end\":", out);
        json_stream_seconds(out, chapter->end, chapter->time_base);
        fputs(",\"metadata\":", out);
        json_dictionary(out, chapter->metadata);
        fputc('}', out);
    }
    fputc(']', out);
}

static int write_report(FILE *out, AVFormatContext *format)
{
    int64_t file_size = -1;
    int64_t bit_rate = format->bit_rate;
    const char *bit_rate_source = bit_rate > 0 ? "container" : NULL;
    unsigned int i;

    if (format->pb)
        file_size = avio_size(format->pb);

    if (bit_rate <= 0 && file_size > 0 &&
        format->duration != AV_NOPTS_VALUE && format->duration > 0) {
        double estimate = ((double)file_size * 8.0 * AV_TIME_BASE) / (double)format->duration;
        if (isfinite(estimate) && estimate > 0.0 && estimate <= (double)INT64_MAX) {
            bit_rate = (int64_t)llround(estimate);
            bit_rate_source = "estimated-from-size-duration";
        }
    }

    fprintf(out, "{\"schemaVersion\":%d,\"runnerVersion\":", REPORT_SCHEMA_VERSION);
    json_string(out, RUNNER_VERSION);
    fputs(",\"ffmpegVersion\":", out);
    json_string(out, av_version_info());

    fputs(",\"format\":{\"name\":", out);
    json_string(out, format->iformat ? format->iformat->name : NULL);
    fputs(",\"longName\":", out);
    json_string(out, format->iformat ? format->iformat->long_name : NULL);
    fputs(",\"fileSize\":", out);
    if (file_size >= 0) fprintf(out, "%" PRId64, file_size);
    else fputs("null", out);
    fputs(",\"duration\":", out);
    json_seconds_from_us(out, format->duration);
    fputs(",\"startTime\":", out);
    json_seconds_from_us(out, format->start_time);
    fputs(",\"bitRate\":", out);
    json_i64_or_null(out, bit_rate);
    fputs(",\"reportedBitRate\":", out);
    json_i64_or_null(out, format->bit_rate);
    fputs(",\"bitRateSource\":", out);
    json_string(out, bit_rate_source);
    fprintf(out,
            ",\"probeScore\":%d,\"streamCount\":%u,\"chapterCount\":%u,"
            "\"programCount\":%u,\"streamGroupCount\":%u,\"metadata\":",
            format->probe_score, format->nb_streams, format->nb_chapters,
            format->nb_programs, format->nb_stream_groups);
    json_dictionary(out, format->metadata);
    fputc('}', out);

    fputs(",\"streams\":[", out);
    for (i = 0; i < format->nb_streams; i++) {
        if (i) fputc(',', out);
        json_stream(out, format, format->streams[i]);
    }
    fputc(']', out);

    fputs(",\"chapters\":", out);
    json_chapters(out, format);
    fputs("}\n", out);

    return ferror(out) ? AVERROR(EIO) : 0;
}

static int run_inspector(const RunnerOptions *options)
{
    AVFormatContext *format = NULL;
    FILE *out = NULL;
    int ret;

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

    out = fopen(options->output_path, "wb");
    if (!out) {
        ret = AVERROR(errno);
        av_log(NULL, AV_LOG_ERROR, "Could not open JSON output: %s\n", av_err2str(ret));
        goto end;
    }

    ret = write_report(out, format);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write JSON report: %s\n", av_err2str(ret));
        goto end;
    }

    if (fclose(out) != 0) {
        out = NULL;
        ret = AVERROR(errno);
        av_log(NULL, AV_LOG_ERROR, "Could not close JSON report: %s\n", av_err2str(ret));
        goto end;
    }
    out = NULL;

    printf("media-inspector: streams=%u chapters=%u format=%s\n",
           format->nb_streams, format->nb_chapters,
           format->iformat && format->iformat->name ? format->iformat->name : "unknown");
    fflush(stdout);
    ret = 0;

end:
    if (out) fclose(out);
    avformat_close_input(&format);
    return ret;
}

int main(int argc, char **argv)
{
    RunnerOptions options;
    int parse_result;
    int ret;

    av_log_set_level(AV_LOG_WARNING);

    parse_result = parse_options(argc, argv, &options);
    if (parse_result > 0)
        return 0;
    if (parse_result < 0)
        return 2;

    ret = run_inspector(&options);
    if (ret < 0) {
        fprintf(stderr, "Media inspection failed: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
