/*
 * FFmpeg WASM Builder - Video Audio Extractor runner.
 *
 * Uses FFmpeg public libavformat/libavcodec APIs to inspect media streams and
 * copy exactly one selected audio stream into a validated audio container.
 * No audio/video decoder, encoder, filter, swscale, or swresample stage is
 * used by this Phase 1 profile.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/bsf.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_desc.h>
#include <libavcodec/codec_id.h>
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

static void print_usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM Video Audio Extractor %s\n"
        "Usage:\n"
        "  %s --input INPUT --inspect-output REPORT.json\n"
        "  %s --input INPUT --audio-stream INDEX --copy-format FORMAT --output OUTPUT\n\n"
        "Copy formats: m4a, mp3, opus, ogg, flac, wav, ac3, eac3\n",
        RUNNER_VERSION, program, program);
}

static int parse_nonnegative_int(const char *value, int *out)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || !end || *end != '\0' || parsed < 0 || parsed > INT32_MAX)
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
            print_usage(argv[0]);
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
        if (!strcmp(arg, "--input")) {
            options->input_path = value;
        } else if (!strcmp(arg, "--inspect-output")) {
            if (options->operation != OP_NONE && options->operation != OP_INSPECT) {
                fprintf(stderr, "Inspection and copy options cannot be combined.\n");
                return AVERROR(EINVAL);
            }
            options->operation = OP_INSPECT;
            options->output_path = value;
        } else if (!strcmp(arg, "--audio-stream")) {
            if (parse_nonnegative_int(value, &options->audio_stream_index) < 0) {
                fprintf(stderr, "Invalid audio stream index: %s\n", value);
                return AVERROR(EINVAL);
            }
        } else if (!strcmp(arg, "--copy-format")) {
            if (options->operation != OP_NONE && options->operation != OP_COPY) {
                fprintf(stderr, "Inspection and copy options cannot be combined.\n");
                return AVERROR(EINVAL);
            }
            options->operation = OP_COPY;
            options->copy_format = value;
        } else if (!strcmp(arg, "--output")) {
            options->output_path = value;
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
    }

    if (!options->input_path) {
        fprintf(stderr, "--input is required.\n");
        return AVERROR(EINVAL);
    }
    if (options->operation == OP_INSPECT) {
        if (!options->output_path) {
            fprintf(stderr, "--inspect-output is required for inspection.\n");
            return AVERROR(EINVAL);
        }
        return 0;
    }
    if (options->operation == OP_COPY) {
        if (!options->output_path || !options->copy_format || options->audio_stream_index < 0) {
            fprintf(stderr, "--audio-stream, --copy-format, and --output are required for stream copy.\n");
            return AVERROR(EINVAL);
        }
        return 0;
    }

    fprintf(stderr, "Choose either --inspect-output or --copy-format.\n");
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

static void json_i64_or_null(FILE *out, int64_t value)
{
    if (value <= 0) fputs("null", out);
    else fprintf(out, "%" PRId64, value);
}

static void json_seconds_from_us(FILE *out, int64_t value)
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

static void codec_tag_string(char *buffer, size_t size, uint32_t tag)
{
    if (!buffer || size == 0) return;
    buffer[0] = '\0';
    if (tag) av_fourcc_make_string(buffer, tag);
}

static const char *metadata_value(const AVDictionary *metadata, const char *key)
{
    const AVDictionaryEntry *entry = av_dict_get(metadata, key, NULL, 0);
    return entry ? entry->value : NULL;
}

static int codec_is_supported_pcm(enum AVCodecID codec_id)
{
    switch (codec_id) {
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_F32LE:
    case AV_CODEC_ID_PCM_F64LE:
    case AV_CODEC_ID_PCM_U8:
    case AV_CODEC_ID_PCM_S8:
        return 1;
    default:
        return 0;
    }
}

static int copy_target_for_codec(enum AVCodecID codec_id, CopyTarget *target)
{
    CopyTarget selected = { 0 };

    switch (codec_id) {
    case AV_CODEC_ID_AAC:
    case AV_CODEC_ID_ALAC:
        selected = (CopyTarget){ "m4a", "m4a", "ipod" };
        break;
    case AV_CODEC_ID_MP3:
        selected = (CopyTarget){ "mp3", "mp3", "mp3" };
        break;
    case AV_CODEC_ID_OPUS:
        selected = (CopyTarget){ "opus", "opus", "ogg" };
        break;
    case AV_CODEC_ID_VORBIS:
        selected = (CopyTarget){ "ogg", "ogg", "ogg" };
        break;
    case AV_CODEC_ID_FLAC:
        selected = (CopyTarget){ "flac", "flac", "flac" };
        break;
    case AV_CODEC_ID_AC3:
        selected = (CopyTarget){ "ac3", "ac3", "ac3" };
        break;
    case AV_CODEC_ID_EAC3:
        selected = (CopyTarget){ "eac3", "eac3", "eac3" };
        break;
    default:
        if (codec_is_supported_pcm(codec_id))
            selected = (CopyTarget){ "wav", "wav", "wav" };
        break;
    }

    if (!selected.format)
        return AVERROR(ENOSYS);
    if (target) *target = selected;
    return 0;
}

static void describe_channel_layout(const AVCodecParameters *par, char *buffer, size_t size)
{
    if (!buffer || size == 0) return;
    buffer[0] = '\0';
    if (!par || par->ch_layout.nb_channels <= 0) return;
    if (av_channel_layout_describe(&par->ch_layout, buffer, size) < 0)
        buffer[0] = '\0';
}

static int write_inspection_report(FILE *out, AVFormatContext *format)
{
    int64_t file_size = -1;
    unsigned int i;
    unsigned int video_count = 0;
    unsigned int audio_count = 0;
    int first_audio = 1;

    if (format->pb)
        file_size = avio_size(format->pb);

    for (i = 0; i < format->nb_streams; i++) {
        AVStream *stream = format->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            audio_count++;
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
                 !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
            video_count++;
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
    fputs(",\"bitRate\":", out);
    json_i64_or_null(out, format->bit_rate);
    fprintf(out, ",\"videoStreamCount\":%u,\"audioStreamCount\":%u}", video_count, audio_count);

    fputs(",\"audioStreams\":[", out);
    for (i = 0; i < format->nb_streams; i++) {
        AVStream *stream = format->streams[i];
        AVCodecParameters *par = stream->codecpar;
        const AVCodecDescriptor *descriptor;
        const char *profile_name = NULL;
        char codec_tag[32];
        char channel_layout[256];
        CopyTarget target = { 0 };
        int copy_supported;

        if (par->codec_type != AVMEDIA_TYPE_AUDIO)
            continue;
        if (!first_audio) fputc(',', out);
        first_audio = 0;

        descriptor = avcodec_descriptor_get(par->codec_id);
        if (par->profile != AV_PROFILE_UNKNOWN)
            profile_name = avcodec_profile_name(par->codec_id, par->profile);
        codec_tag_string(codec_tag, sizeof(codec_tag), par->codec_tag);
        describe_channel_layout(par, channel_layout, sizeof(channel_layout));
        copy_supported = copy_target_for_codec(par->codec_id, &target) == 0;

        fprintf(out, "{\"index\":%d,\"codec\":{\"name\":", stream->index);
        json_string(out, avcodec_get_name(par->codec_id));
        fputs(",\"longName\":", out);
        json_string(out, descriptor ? descriptor->long_name : NULL);
        fputs(",\"tag\":", out);
        json_string(out, codec_tag[0] ? codec_tag : NULL);
        fputs(",\"profile\":", out);
        json_string(out, profile_name);
        fputs(",\"bitRate\":", out);
        json_i64_or_null(out, par->bit_rate);
        fputs("},\"sampleRate\":", out);
        fprintf(out, "%d", par->sample_rate);
        fputs(",\"channels\":", out);
        fprintf(out, "%d", par->ch_layout.nb_channels);
        fputs(",\"channelLayout\":", out);
        json_string(out, channel_layout[0] ? channel_layout : NULL);
        fputs(",\"language\":", out);
        json_string(out, metadata_value(stream->metadata, "language"));
        fputs(",\"title\":", out);
        json_string(out, metadata_value(stream->metadata, "title"));
        fprintf(out, ",\"default\":%s,\"duration\":",
                (stream->disposition & AV_DISPOSITION_DEFAULT) ? "true" : "false");
        json_stream_seconds(out, stream->duration, stream->time_base);
        fprintf(out, ",\"copy\":{\"supported\":%s,\"format\":",
                copy_supported ? "true" : "false");
        json_string(out, copy_supported ? target.format : NULL);
        fputs(",\"extension\":", out);
        json_string(out, copy_supported ? target.extension : NULL);
        fputs("}}", out);
    }
    fputs("]}\n", out);
    return ferror(out) ? AVERROR(EIO) : 0;
}

static int run_inspection(const RunnerOptions *options)
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
        av_log(NULL, AV_LOG_ERROR, "Could not open inspection output: %s\n", av_err2str(ret));
        goto end;
    }
    ret = write_inspection_report(out, format);
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
    if (out) fclose(out);
    avformat_close_input(&format);
    return ret;
}

static int input_needs_aac_adtstoasc(const AVFormatContext *input,
                                      const AVStream *stream,
                                      const char *copy_format)
{
    const char *name;

    if (!input || !stream || !copy_format || strcmp(copy_format, "m4a"))
        return 0;
    if (stream->codecpar->codec_id != AV_CODEC_ID_AAC)
        return 0;
    name = input->iformat ? input->iformat->name : NULL;
    if (!name) return 0;
    return strstr(name, "mpegts") != NULL || !strcmp(name, "aac");
}

static int init_copy_bsf(AVBSFContext **out_bsf,
                         const AVFormatContext *input,
                         const AVStream *stream,
                         const char *copy_format)
{
    const AVBitStreamFilter *filter;
    AVBSFContext *bsf = NULL;
    int ret;

    *out_bsf = NULL;
    if (!input_needs_aac_adtstoasc(input, stream, copy_format))
        return 0;

    filter = av_bsf_get_by_name("aac_adtstoasc");
    if (!filter)
        return AVERROR(ENOSYS);
    ret = av_bsf_alloc(filter, &bsf);
    if (ret < 0) return ret;
    ret = avcodec_parameters_copy(bsf->par_in, stream->codecpar);
    if (ret < 0) goto fail;
    bsf->time_base_in = stream->time_base;
    ret = av_bsf_init(bsf);
    if (ret < 0) goto fail;
    *out_bsf = bsf;
    return 0;

fail:
    av_bsf_free(&bsf);
    return ret;
}

static void emit_progress(const AVPacket *packet,
                          const AVStream *stream,
                          int64_t format_start_time,
                          int64_t format_duration,
                          double *last_progress)
{
    int64_t ts;
    int64_t packet_us;
    double progress;

    if (!packet || !stream || format_duration <= 0) return;
    ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (ts == AV_NOPTS_VALUE) return;
    packet_us = av_rescale_q(ts, stream->time_base, AV_TIME_BASE_Q);
    if (format_start_time != AV_NOPTS_VALUE)
        packet_us -= format_start_time;
    progress = (double)packet_us / (double)format_duration;
    if (!isfinite(progress)) return;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    if (progress >= *last_progress + 0.01 || progress >= 1.0) {
        printf(PROGRESS_PREFIX " %.6f\n", progress);
        fflush(stdout);
        *last_progress = progress;
    }
}

static int write_filtered_packets(AVFormatContext *output,
                                  AVBSFContext *bsf,
                                  AVStream *out_stream,
                                  AVPacket *packet,
                                  int *packets_written)
{
    int ret;

    ret = av_bsf_send_packet(bsf, packet);
    if (ret < 0) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        packet->stream_index = out_stream->index;
        packet->pos = -1;
        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) return ret;
        (*packets_written)++;
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return 0;
    return ret;
}

static int flush_filtered_packets(AVFormatContext *output,
                                  AVBSFContext *bsf,
                                  AVStream *out_stream,
                                  AVPacket *packet,
                                  int *packets_written)
{
    int ret;

    ret = av_bsf_send_packet(bsf, NULL);
    if (ret < 0 && ret != AVERROR_EOF) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        packet->stream_index = out_stream->index;
        packet->pos = -1;
        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) return ret;
        (*packets_written)++;
    }
    return ret == AVERROR_EOF || ret == AVERROR(EAGAIN) ? 0 : ret;
}

static int run_copy(const RunnerOptions *options)
{
    AVFormatContext *input = NULL;
    AVFormatContext *output = NULL;
    AVStream *in_stream;
    AVStream *out_stream = NULL;
    AVPacket *packet = NULL;
    AVBSFContext *bsf = NULL;
    CopyTarget target = { 0 };
    double last_progress = -1.0;
    int header_written = 0;
    int packets_written = 0;
    int ret;

    ret = avformat_open_input(&input, options->input_path, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open input: %s\n", av_err2str(ret));
        goto end;
    }
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not read stream information: %s\n", av_err2str(ret));
        goto end;
    }
    if (options->audio_stream_index < 0 ||
        (unsigned int)options->audio_stream_index >= input->nb_streams) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Selected audio stream does not exist.\n");
        goto end;
    }
    in_stream = input->streams[options->audio_stream_index];
    if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Selected stream is not audio.\n");
        goto end;
    }

    ret = copy_target_for_codec(in_stream->codecpar->codec_id, &target);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Selected audio codec is not approved for stream copy.\n");
        goto end;
    }
    if (strcmp(options->copy_format, target.format)) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR,
               "Copy format '%s' is not valid for codec %s; expected '%s'.\n",
               options->copy_format, avcodec_get_name(in_stream->codecpar->codec_id), target.format);
        goto end;
    }

    ret = init_copy_bsf(&bsf, input, in_stream, options->copy_format);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not prepare AAC container conversion: %s\n", av_err2str(ret));
        goto end;
    }

    ret = avformat_alloc_output_context2(&output, NULL, target.muxer, options->output_path);
    if (ret < 0 || !output) {
        if (ret >= 0) ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Could not create output container: %s\n", av_err2str(ret));
        goto end;
    }
    if (avformat_query_codec(output->oformat, in_stream->codecpar->codec_id, FF_COMPLIANCE_NORMAL) == 0) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Output container rejected the selected codec.\n");
        goto end;
    }

    out_stream = avformat_new_stream(output, NULL);
    if (!out_stream) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_parameters_copy(out_stream->codecpar, bsf ? bsf->par_out : in_stream->codecpar);
    if (ret < 0) goto end;
    out_stream->codecpar->codec_tag = 0;
    out_stream->time_base = (bsf && bsf->time_base_out.num > 0 && bsf->time_base_out.den > 0)
        ? bsf->time_base_out : in_stream->time_base;
    out_stream->disposition = in_stream->disposition;
    av_dict_copy(&out_stream->metadata, in_stream->metadata, 0);
    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output->pb, options->output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output: %s\n", av_err2str(ret));
            goto end;
        }
    }
    ret = avformat_write_header(output, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not write output header: %s\n", av_err2str(ret));
        goto end;
    }
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
        emit_progress(packet, in_stream, input->start_time, input->duration, &last_progress);
        if (bsf) {
            ret = write_filtered_packets(output, bsf, out_stream, packet, &packets_written);
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
        ret = flush_filtered_packets(output, bsf, out_stream, packet, &packets_written);
        if (ret < 0) goto end;
    }
    if (packets_written <= 0) {
        ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR, "Selected audio stream produced no output packets.\n");
        goto end;
    }

    ret = av_write_trailer(output);
    if (ret < 0) goto end;
    header_written = 0;
    printf(PROGRESS_PREFIX " 1.000000\n");
    printf("video-audio-extractor: copied stream=%d codec=%s format=%s packets=%d\n",
           options->audio_stream_index, avcodec_get_name(in_stream->codecpar->codec_id),
           target.format, packets_written);
    fflush(stdout);

end:
    if (ret < 0 && header_written && output)
        av_write_trailer(output);
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
    if (parsed < 0) return 2;

    if (options.operation == OP_INSPECT)
        ret = run_inspection(&options);
    else
        ret = run_copy(&options);

    if (ret < 0) {
        fprintf(stderr, "Video Audio Extractor failed: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
