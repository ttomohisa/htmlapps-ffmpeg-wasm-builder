/*
 * FFmpeg WASM Builder - Video Audio Extractor runner.
 *
 * Phase 1 uses public libavformat/libavcodec APIs to inspect streams and copy
 * one selected compressed audio stream into an approved audio container.
 * No decoder, encoder, filter, swscale, or swresample stage is used.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/bsf.h>
#include <libavcodec/codec_desc.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>

#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define RUNNER_VERSION "1.0.0"
#define REPORT_SCHEMA_VERSION 1

typedef enum RunnerOperation {
    OP_NONE = 0,
    OP_INSPECT,
    OP_COPY
} RunnerOperation;

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    const char *copy_format;
    int audio_stream_index;
    RunnerOperation operation;
} RunnerOptions;

typedef struct CopyTarget {
    const char *format;
    const char *extension;
    const char *muxer;
} CopyTarget;

static void usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM Video Audio Extractor %s\n"
        "Usage:\n"
        "  %s --input INPUT --inspect-output REPORT.json\n"
        "  %s --input INPUT --audio-stream INDEX --copy-format FORMAT --output OUTPUT\n",
        RUNNER_VERSION, program, program);
}

static int parse_index(const char *value, int *out)
{
    char *end = NULL;
    long parsed;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || !end || *end || parsed < 0 || parsed > INT32_MAX)
        return AVERROR(EINVAL);
    *out = (int)parsed;
    return 0;
}

static int parse_options(int argc, char **argv, RunnerOptions *options)
{
    int i;
    memset(options, 0, sizeof(*options));
    options->audio_stream_index = -1;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            usage(argv[0]);
            return 1;
        }
        if (!strcmp(arg, "--version")) {
            printf("video-audio-extractor %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
            return 1;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return AVERROR(EINVAL);
        }
        value = argv[++i];

        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--inspect-output")) {
            if (options->operation != OP_NONE && options->operation != OP_INSPECT)
                return AVERROR(EINVAL);
            options->operation = OP_INSPECT;
            options->output_path = value;
        } else if (!strcmp(arg, "--audio-stream")) {
            if (parse_index(value, &options->audio_stream_index) < 0)
                return AVERROR(EINVAL);
        } else if (!strcmp(arg, "--copy-format")) {
            if (options->operation != OP_NONE && options->operation != OP_COPY)
                return AVERROR(EINVAL);
            options->operation = OP_COPY;
            options->copy_format = value;
        } else if (!strcmp(arg, "--output")) options->output_path = value;
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
    }

    if (!options->input_path) return AVERROR(EINVAL);
    if (options->operation == OP_INSPECT)
        return options->output_path ? 0 : AVERROR(EINVAL);
    if (options->operation == OP_COPY)
        return options->output_path && options->copy_format &&
               options->audio_stream_index >= 0 ? 0 : AVERROR(EINVAL);
    return AVERROR(EINVAL);
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
            if (*p < 0x20) fprintf(out, "\\u%04x", (unsigned)*p);
            else fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void json_i64(FILE *out, int64_t value)
{
    if (value <= 0) fputs("null", out);
    else fprintf(out, "%" PRId64, value);
}

static void json_seconds_us(FILE *out, int64_t value)
{
    if (value == AV_NOPTS_VALUE || value < 0) fputs("null", out);
    else fprintf(out, "%.6f", (double)value / AV_TIME_BASE);
}

static void json_stream_seconds(FILE *out, int64_t value, AVRational time_base)
{
    if (value == AV_NOPTS_VALUE || value < 0 || time_base.num <= 0 || time_base.den <= 0)
        fputs("null", out);
    else
        fprintf(out, "%.6f", value * av_q2d(time_base));
}

static const char *metadata_value(const AVDictionary *metadata, const char *key)
{
    const AVDictionaryEntry *entry = av_dict_get(metadata, key, NULL, 0);
    return entry ? entry->value : NULL;
}

static int copy_target(enum AVCodecID id, CopyTarget *target)
{
    CopyTarget selected = {0};
    switch (id) {
    case AV_CODEC_ID_AAC:
        selected = (CopyTarget){"m4a", "m4a", "ipod"};
        break;
    case AV_CODEC_ID_OPUS:
        selected = (CopyTarget){"opus", "opus", "ogg"};
        break;
    default:
        break;
    }
    if (!selected.format) return AVERROR(ENOSYS);
    if (target) *target = selected;
    return 0;
}

static void channel_layout_text(const AVCodecParameters *par, char *text, size_t size)
{
    text[0] = '\0';
    if (!par || par->ch_layout.nb_channels <= 0) return;
    if (av_channel_layout_describe(&par->ch_layout, text, size) < 0)
        text[0] = '\0';
}

static int write_report(FILE *out, AVFormatContext *format)
{
    int64_t file_size = format->pb ? avio_size(format->pb) : -1;
    unsigned int video_count = 0;
    unsigned int audio_count = 0;
    unsigned int i;
    int first = 1;

    for (i = 0; i < format->nb_streams; i++) {
        AVStream *stream = format->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audio_count++;
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                 !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC)) video_count++;
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
    json_seconds_us(out, format->duration);
    fputs(",\"bitRate\":", out);
    json_i64(out, format->bit_rate);
    fprintf(out, ",\"videoStreamCount\":%u,\"audioStreamCount\":%u}",
            video_count, audio_count);

    fputs(",\"audioStreams\":[", out);
    for (i = 0; i < format->nb_streams; i++) {
        AVStream *stream = format->streams[i];
        AVCodecParameters *par = stream->codecpar;
        const AVCodecDescriptor *descriptor;
        const char *profile = NULL;
        char tag[32] = {0};
        char layout[256];
        CopyTarget target = {0};
        int can_copy;

        if (par->codec_type != AVMEDIA_TYPE_AUDIO) continue;
        if (!first) fputc(',', out);
        first = 0;

        descriptor = avcodec_descriptor_get(par->codec_id);
        if (par->profile != AV_PROFILE_UNKNOWN)
            profile = avcodec_profile_name(par->codec_id, par->profile);
        if (par->codec_tag) av_fourcc_make_string(tag, par->codec_tag);
        channel_layout_text(par, layout, sizeof(layout));
        can_copy = copy_target(par->codec_id, &target) == 0;

        fprintf(out, "{\"index\":%d,\"codec\":{\"name\":", stream->index);
        json_string(out, avcodec_get_name(par->codec_id));
        fputs(",\"longName\":", out);
        json_string(out, descriptor ? descriptor->long_name : NULL);
        fputs(",\"tag\":", out);
        json_string(out, tag[0] ? tag : NULL);
        fputs(",\"profile\":", out);
        json_string(out, profile);
        fputs(",\"bitRate\":", out);
        json_i64(out, par->bit_rate);
        fprintf(out, "},\"sampleRate\":%d,\"channels\":%d,\"channelLayout\":",
                par->sample_rate, par->ch_layout.nb_channels);
        json_string(out, layout[0] ? layout : NULL);
        fputs(",\"language\":", out);
        json_string(out, metadata_value(stream->metadata, "language"));
        fputs(",\"title\":", out);
        json_string(out, metadata_value(stream->metadata, "title"));
        fprintf(out, ",\"default\":%s,\"duration\":",
                (stream->disposition & AV_DISPOSITION_DEFAULT) ? "true" : "false");
        json_stream_seconds(out, stream->duration, stream->time_base);
        fprintf(out, ",\"copy\":{\"supported\":%s,\"format\":",
                can_copy ? "true" : "false");
        json_string(out, can_copy ? target.format : NULL);
        fputs(",\"extension\":", out);
        json_string(out, can_copy ? target.extension : NULL);
        fputs("}}", out);
    }
    fputs("]}\n", out);
    return ferror(out) ? AVERROR(EIO) : 0;
}

static int inspect_media(const RunnerOptions *options)
{
    AVFormatContext *format = NULL;
    FILE *out = NULL;
    int ret;

    ret = avformat_open_input(&format, options->input_path, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(format, NULL);
    if (ret < 0) goto end;

    out = fopen(options->output_path, "wb");
    if (!out) {
        ret = AVERROR(errno);
        goto end;
    }
    ret = write_report(out, format);
    if (ret < 0) goto end;
    if (fclose(out) != 0) {
        out = NULL;
        ret = AVERROR(errno);
        goto end;
    }
    out = NULL;
    printf("video-audio-extractor: inspect streams=%u\n", format->nb_streams);
    fflush(stdout);
    ret = 0;

end:
    if (ret < 0) av_log(NULL, AV_LOG_ERROR, "Inspection failed: %s\n", av_err2str(ret));
    if (out) fclose(out);
    avformat_close_input(&format);
    return ret;
}

static int needs_aac_adtstoasc(const AVFormatContext *input,
                               const AVStream *stream,
                               const char *format)
{
    const char *name;
    if (!input || !stream || !format || strcmp(format, "m4a")) return 0;
    if (stream->codecpar->codec_id != AV_CODEC_ID_AAC) return 0;
    name = input->iformat ? input->iformat->name : NULL;
    return name && (strstr(name, "mpegts") || !strcmp(name, "aac"));
}

static int create_bsf(AVBSFContext **result,
                      const AVFormatContext *input,
                      const AVStream *stream,
                      const char *format)
{
    const AVBitStreamFilter *filter;
    AVBSFContext *bsf = NULL;
    int ret;

    *result = NULL;
    if (!needs_aac_adtstoasc(input, stream, format)) return 0;

    filter = av_bsf_get_by_name("aac_adtstoasc");
    if (!filter) return AVERROR(ENOENT);
    ret = av_bsf_alloc(filter, &bsf);
    if (ret < 0) return ret;
    ret = avcodec_parameters_copy(bsf->par_in, stream->codecpar);
    if (ret < 0) goto fail;
    bsf->time_base_in = stream->time_base;
    ret = av_bsf_init(bsf);
    if (ret < 0) goto fail;
    *result = bsf;
    return 0;
fail:
    av_bsf_free(&bsf);
    return ret;
}

static void emit_progress(const AVPacket *packet,
                          const AVStream *stream,
                          int64_t duration,
                          double *last)
{
    int64_t ts;
    int64_t usec;
    double progress;
    if (duration <= 0) return;
    ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (ts == AV_NOPTS_VALUE) return;
    usec = av_rescale_q(ts, stream->time_base, AV_TIME_BASE_Q);
    progress = (double)usec / (double)duration;
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    if (progress >= *last + 0.01 || progress >= 1) {
        printf(PROGRESS_PREFIX " %.6f\n", progress);
        fflush(stdout);
        *last = progress;
    }
}

static int write_bsf_packets(AVFormatContext *output,
                             AVBSFContext *bsf,
                             AVStream *out_stream,
                             AVPacket *packet,
                             int *count)
{
    int ret = av_bsf_send_packet(bsf, packet);
    if (ret < 0) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        packet->stream_index = out_stream->index;
        packet->pos = -1;
        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) return ret;
        (*count)++;
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

static int flush_bsf(AVFormatContext *output,
                     AVBSFContext *bsf,
                     AVStream *out_stream,
                     AVPacket *packet,
                     int *count)
{
    int ret = av_bsf_send_packet(bsf, NULL);
    if (ret < 0 && ret != AVERROR_EOF) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        packet->stream_index = out_stream->index;
        packet->pos = -1;
        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) return ret;
        (*count)++;
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

static int copy_audio(const RunnerOptions *options)
{
    AVFormatContext *input = NULL;
    AVFormatContext *output = NULL;
    AVStream *in_stream;
    AVStream *out_stream = NULL;
    AVPacket *packet = NULL;
    AVBSFContext *bsf = NULL;
    CopyTarget target = {0};
    double last_progress = -1;
    int header_written = 0;
    int packets_written = 0;
    int ret;

    ret = avformat_open_input(&input, options->input_path, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0) goto end;

    if ((unsigned int)options->audio_stream_index >= input->nb_streams) {
        ret = AVERROR(EINVAL);
        goto end;
    }
    in_stream = input->streams[options->audio_stream_index];
    if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        ret = AVERROR(EINVAL);
        goto end;
    }
    ret = copy_target(in_stream->codecpar->codec_id, &target);
    if (ret < 0) goto end;
    if (strcmp(options->copy_format, target.format)) {
        ret = AVERROR(EINVAL);
        goto end;
    }

    ret = create_bsf(&bsf, input, in_stream, target.format);
    if (ret < 0) goto end;

    ret = avformat_alloc_output_context2(&output, NULL, target.muxer, options->output_path);
    if (ret < 0 || !output) {
        if (ret >= 0) ret = AVERROR(EINVAL);
        goto end;
    }
    if (avformat_query_codec(output->oformat, in_stream->codecpar->codec_id,
                             FF_COMPLIANCE_NORMAL) == 0) {
        ret = AVERROR(EINVAL);
        goto end;
    }

    out_stream = avformat_new_stream(output, NULL);
    if (!out_stream) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_parameters_copy(out_stream->codecpar,
                                  bsf ? bsf->par_out : in_stream->codecpar);
    if (ret < 0) goto end;
    out_stream->codecpar->codec_tag = 0;
    out_stream->time_base = bsf ? bsf->time_base_out : in_stream->time_base;
    out_stream->disposition = in_stream->disposition;
    av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);
    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output->pb, options->output_path, AVIO_FLAG_WRITE);
        if (ret < 0) goto end;
    }
    ret = avformat_write_header(output, NULL);
    if (ret < 0) goto end;
    header_written = 1;

    packet = av_packet_alloc();
    if (!packet) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while ((ret = av_read_frame(input, packet)) >= 0) {
        if (packet->stream_index != options->audio_stream_index) {
            av_packet_unref(packet);
            continue;
        }
        emit_progress(packet, in_stream, input->duration, &last_progress);
        if (bsf) {
            ret = write_bsf_packets(output, bsf, out_stream, packet, &packets_written);
            av_packet_unref(packet);
            if (ret < 0) goto end;
        } else {
            av_packet_rescale_ts(packet, in_stream->time_base, out_stream->time_base);
            packet->stream_index = out_stream->index;
            packet->pos = -1;
            ret = av_interleaved_write_frame(output, packet);
            av_packet_unref(packet);
            if (ret < 0) goto end;
            packets_written++;
        }
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret < 0) goto end;
    if (bsf) {
        ret = flush_bsf(output, bsf, out_stream, packet, &packets_written);
        if (ret < 0) goto end;
    }
    if (packets_written <= 0) {
        ret = AVERROR(EINVAL);
        goto end;
    }

    ret = av_write_trailer(output);
    if (ret < 0) goto end;
    header_written = 0;
    printf(PROGRESS_PREFIX " 1.000000\n");
    printf("video-audio-extractor: stream=%d codec=%s format=%s packets=%d\n",
           options->audio_stream_index,
           avcodec_get_name(in_stream->codecpar->codec_id),
           target.format, packets_written);
    fflush(stdout);

end:
    if (ret < 0) av_log(NULL, AV_LOG_ERROR, "Audio stream copy failed: %s\n", av_err2str(ret));
    if (ret < 0 && header_written && output) av_write_trailer(output);
    av_packet_free(&packet);
    av_bsf_free(&bsf);
    if (input) avformat_close_input(&input);
    if (output) {
        if (!(output->oformat->flags & AVFMT_NOFILE) && output->pb)
            avio_closep(&output->pb);
        avformat_free_context(output);
    }
    return ret;
}

int main(int argc, char **argv)
{
    RunnerOptions options;
    int parsed;
    int ret;

    av_log_set_level(AV_LOG_WARNING);
    parsed = parse_options(argc, argv, &options);
    if (parsed > 0) return 0;
    if (parsed < 0) {
        usage(argv[0]);
        return 2;
    }

    ret = options.operation == OP_INSPECT ? inspect_media(&options) : copy_audio(&options);
    if (ret < 0) {
        fprintf(stderr, "Video Audio Extractor failed: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
