/*
 * FFmpeg WASM Builder - Video Audio Extractor runner.
 *
 * Phase 2 uses public libavformat/libavcodec APIs to inspect streams and copy
 * one selected compressed audio stream into an approved audio container.
 * No decoder, encoder, filter, swscale, or swresample stage is used.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavcodec/codec_desc.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define RUNNER_VERSION "1.2.0"
#define REPORT_SCHEMA_VERSION 1

typedef enum RunnerOperation {
    OP_NONE = 0,
    OP_INSPECT,
    OP_COPY,
    OP_TRANSCODE
} RunnerOperation;

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    const char *copy_format;
    const char *transcode_format;
    int bitrate_kbps;
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
        "  %s --input INPUT --audio-stream INDEX --copy-format FORMAT --output OUTPUT\n"
        "  %s --input INPUT --audio-stream INDEX --transcode-format m4a|wav [--bitrate-kbps 128|192|256] --output OUTPUT\n",
        RUNNER_VERSION, program, program, program);
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
        } else if (!strcmp(arg, "--transcode-format")) {
            if (options->operation != OP_NONE && options->operation != OP_TRANSCODE)
                return AVERROR(EINVAL);
            options->operation = OP_TRANSCODE;
            options->transcode_format = value;
        } else if (!strcmp(arg, "--bitrate-kbps")) {
            if (parse_index(value, &options->bitrate_kbps) < 0)
                return AVERROR(EINVAL);
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
    if (options->operation == OP_TRANSCODE)
        return options->output_path && options->transcode_format &&
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
    case AV_CODEC_ID_ALAC:
        selected = (CopyTarget){"m4a", "m4a", "ipod"};
        break;
    case AV_CODEC_ID_OPUS:
        selected = (CopyTarget){"opus", "opus", "ogg"};
        break;
    case AV_CODEC_ID_VORBIS:
        selected = (CopyTarget){"ogg", "ogg", "ogg"};
        break;
    case AV_CODEC_ID_MP3:
        selected = (CopyTarget){"mp3", "mp3", "mp3"};
        break;
    case AV_CODEC_ID_FLAC:
        selected = (CopyTarget){"flac", "flac", "flac"};
        break;
    default:
        break;
    }
    if (!selected.format) return AVERROR(ENOSYS);
    if (target) *target = selected;
    return 0;
}

static int transcode_source_supported(enum AVCodecID id)
{
    switch (id) {
    case AV_CODEC_ID_AAC:
    case AV_CODEC_ID_ALAC:
    case AV_CODEC_ID_MP3:
    case AV_CODEC_ID_OPUS:
    case AV_CODEC_ID_VORBIS:
    case AV_CODEC_ID_FLAC:
        return 1;
    default:
        return 0;
    }
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
        fprintf(out, "},\"transcode\":{\"supported\":%s,\"formats\":[\"m4a\",\"wav\"]}}",
                transcode_source_supported(par->codec_id) ? "true" : "false");
    }
    fputs("]}\n", out);
    return ferror(out) ? AVERROR(EIO) : 0;
}

static int fill_missing_adts_parameters(const char *input_path,
                                        int stream_index,
                                        AVCodecParameters *parameters);

static int inspect_media(const RunnerOptions *options)
{
    AVFormatContext *format = NULL;
    FILE *out = NULL;
    int ret;

    ret = avformat_open_input(&format, options->input_path, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(format, NULL);
    if (ret < 0) goto end;

    /*
     * A decoder-free MPEG-TS probe can identify AAC while leaving sample
     * rate/channel layout unset. Recover those fields from the ADTS header so
     * inspection and later M4A stream copy report the same usable metadata.
     */
    if (format->iformat && strstr(format->iformat->name, "mpegts")) {
        unsigned int i;
        for (i = 0; i < format->nb_streams; i++) {
            AVStream *stream = format->streams[i];
            AVCodecParameters *par = stream->codecpar;
            if (par->codec_type != AVMEDIA_TYPE_AUDIO ||
                par->codec_id != AV_CODEC_ID_AAC ||
                (par->sample_rate > 0 && par->ch_layout.nb_channels > 0))
                continue;
            ret = fill_missing_adts_parameters(options->input_path, stream->index, par);
            if (ret < 0) goto end;
        }
    }

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

static int64_t audio_packet_step(const AVPacket *packet, const AVStream *stream)
{
    int64_t expected = 0;

    /*
     * MPEG-TS may expose a placeholder packet duration of 1 tick when this
     * decoder-free profile cannot derive AAC timing through a decoder.  That
     * is far too small: rescaling 1/90000 to an M4A 48 kHz time base can map
     * adjacent packets to the same DTS.  Prefer one AAC-LC frame (1024
     * samples) and only trust a demuxer duration when it is in the same order
     * of magnitude.
     */
    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC &&
        stream->codecpar->sample_rate > 0) {
        expected = av_rescale_q(1024,
                                (AVRational){1, stream->codecpar->sample_rate},
                                stream->time_base);
        if (expected < 1) expected = 1;
        if (packet->duration >= FFMAX(INT64_C(1), expected / 2))
            return packet->duration;
        return expected;
    }

    return packet->duration > 0 ? packet->duration : 1;
}

static void repair_mpegts_aac_timestamps(const AVFormatContext *input,
                                         const AVStream *stream,
                                         AVPacket *packet,
                                         int64_t *last_dts,
                                         int64_t *next_dts)
{
    int64_t step;
    int repair = needs_aac_adtstoasc(input, stream, "m4a");
    if (!repair) return;

    step = audio_packet_step(packet, stream);
    if (packet->dts == AV_NOPTS_VALUE && packet->pts != AV_NOPTS_VALUE)
        packet->dts = packet->pts;
    if (packet->pts == AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE)
        packet->pts = packet->dts;

    if (packet->dts == AV_NOPTS_VALUE) {
        packet->dts = *next_dts != AV_NOPTS_VALUE ? *next_dts : 0;
        packet->pts = packet->dts;
    } else if (*last_dts != AV_NOPTS_VALUE &&
               packet->dts < *last_dts + FFMAX(INT64_C(1), step / 2)) {
        /*
         * A merely increasing MPEG-TS DTS can still collapse after rescaling
         * to the output time base.  Reject implausibly small AAC spacing here.
         */
        packet->dts = *last_dts + step;
        packet->pts = packet->dts;
    }

    if (packet->pts == AV_NOPTS_VALUE || packet->pts < packet->dts)
        packet->pts = packet->dts;
    if (packet->duration <= 0)
        packet->duration = step;

    *last_dts = packet->dts;
    *next_dts = packet->dts + step;
}

static int fill_missing_adts_parameters(const char *input_path,
                                        int stream_index,
                                        AVCodecParameters *parameters)
{
    static const int sample_rates[13] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350
    };
    AVFormatContext *probe = NULL;
    AVPacket *packet = NULL;
    int ret;
    int packets_seen = 0;

    if (!parameters || parameters->codec_id != AV_CODEC_ID_AAC)
        return 0;
    if (parameters->sample_rate > 0 && parameters->ch_layout.nb_channels > 0)
        return 0;

    ret = avformat_open_input(&probe, input_path, NULL, NULL);
    if (ret < 0) return ret;

    packet = av_packet_alloc();
    if (!packet) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while (packets_seen++ < 512 && (ret = av_read_frame(probe, packet)) >= 0) {
        int offset;
        if (packet->stream_index != stream_index || packet->size < 7) {
            av_packet_unref(packet);
            continue;
        }
        for (offset = 0; offset + 7 <= packet->size; offset++) {
            const uint8_t *data = packet->data + offset;
            int sample_index;
            int channels;

            if (data[0] != 0xff || (data[1] & 0xf6) != 0xf0)
                continue;
            sample_index = (data[2] >> 2) & 0x0f;
            channels = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);
            if (sample_index >= 13 || channels <= 0)
                continue;

            if (parameters->sample_rate <= 0)
                parameters->sample_rate = sample_rates[sample_index];
            if (parameters->ch_layout.nb_channels <= 0) {
                av_channel_layout_uninit(&parameters->ch_layout);
                av_channel_layout_default(&parameters->ch_layout, channels);
            }
            ret = 0;
            av_packet_unref(packet);
            goto end;
        }
        av_packet_unref(packet);
    }

    if (ret == AVERROR_EOF) ret = AVERROR_INVALIDDATA;
    if (ret >= 0) ret = AVERROR_INVALIDDATA;

end:
    av_packet_free(&packet);
    avformat_close_input(&probe);
    return ret;
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

static void repair_output_aac_timestamps(AVStream *stream,
                                         AVPacket *packet,
                                         int64_t *last_dts)
{
    int64_t step = 1;

    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC &&
        stream->codecpar->sample_rate > 0) {
        step = av_rescale_q(1024,
                            (AVRational){1, stream->codecpar->sample_rate},
                            stream->time_base);
        if (step < 1) step = 1;
    }

    if (packet->dts == AV_NOPTS_VALUE && packet->pts != AV_NOPTS_VALUE)
        packet->dts = packet->pts;
    if (packet->pts == AV_NOPTS_VALUE && packet->dts != AV_NOPTS_VALUE)
        packet->pts = packet->dts;

    if (packet->dts == AV_NOPTS_VALUE) {
        packet->dts = *last_dts == AV_NOPTS_VALUE ? 0 : *last_dts + step;
        packet->pts = packet->dts;
    } else if (*last_dts != AV_NOPTS_VALUE && packet->dts <= *last_dts) {
        /*
         * aac_adtstoasc can emit more than one packet for one MPEG-TS packet.
         * Those packets may inherit an identical DTS.  Repair after the BSF
         * and after time-base rescaling, where the muxer's monotonicity
         * requirement actually applies.
         */
        packet->dts = *last_dts + step;
        packet->pts = packet->dts;
    }

    if (packet->pts == AV_NOPTS_VALUE || packet->pts < packet->dts)
        packet->pts = packet->dts;
    if (packet->duration <= 0)
        packet->duration = step;

    *last_dts = packet->dts;
}

static int write_bsf_packets(AVFormatContext *output,
                             AVBSFContext *bsf,
                             AVStream *out_stream,
                             AVPacket *packet,
                             int64_t *last_output_dts,
                             int *count)
{
    int ret = av_bsf_send_packet(bsf, packet);
    if (ret < 0) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        repair_output_aac_timestamps(out_stream, packet, last_output_dts);
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
                     int64_t *last_output_dts,
                     int *count)
{
    int ret = av_bsf_send_packet(bsf, NULL);
    if (ret < 0 && ret != AVERROR_EOF) return ret;
    while ((ret = av_bsf_receive_packet(bsf, packet)) >= 0) {
        av_packet_rescale_ts(packet, bsf->time_base_out, out_stream->time_base);
        repair_output_aac_timestamps(out_stream, packet, last_output_dts);
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
    int64_t last_input_dts = AV_NOPTS_VALUE;
    int64_t next_input_dts = AV_NOPTS_VALUE;
    int64_t last_output_dts = AV_NOPTS_VALUE;
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

    if (needs_aac_adtstoasc(input, in_stream, target.format) &&
        (in_stream->codecpar->sample_rate <= 0 ||
         in_stream->codecpar->ch_layout.nb_channels <= 0)) {
        ret = fill_missing_adts_parameters(options->input_path,
                                           options->audio_stream_index,
                                           in_stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Could not read AAC parameters from ADTS packets: %s\n",
                   av_err2str(ret));
            goto end;
        }
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
        repair_mpegts_aac_timestamps(input, in_stream, packet,
                                     &last_input_dts, &next_input_dts);
        if (bsf) {
            ret = write_bsf_packets(output, bsf, out_stream, packet,
                                    &last_output_dts, &packets_written);
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
        ret = flush_bsf(output, bsf, out_stream, packet,
                        &last_output_dts, &packets_written);
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


static int choose_audio_sample_rate(const AVCodec *codec, int preferred)
{
    const int *rates = NULL;
    int count = 0;
    int i;
    int best = preferred > 0 ? preferred : 48000;
    int selected = best;
    int best_delta = INT32_MAX;

    if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_RATE,
                                     0, (const void **)&rates, &count) < 0 ||
        !rates || count <= 0)
        return best;

    for (i = 0; i < count; i++) {
        int delta = abs(rates[i] - best);
        if (delta < best_delta) {
            best_delta = delta;
            selected = rates[i];
        }
    }
    return selected;
}

static enum AVSampleFormat choose_audio_sample_format(const AVCodec *codec,
                                                       enum AVSampleFormat preferred)
{
    const enum AVSampleFormat *formats = NULL;
    int count = 0;
    int i;

    if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT,
                                     0, (const void **)&formats, &count) < 0 ||
        !formats || count <= 0)
        return preferred;

    for (i = 0; i < count; i++)
        if (formats[i] == preferred)
            return preferred;
    return formats[0];
}

static int drain_audio_encoder(AVFormatContext *output,
                               AVCodecContext *encoder,
                               AVStream *out_stream,
                               AVPacket *packet)
{
    int ret;
    while ((ret = avcodec_receive_packet(encoder, packet)) >= 0) {
        packet->stream_index = out_stream->index;
        av_packet_rescale_ts(packet, encoder->time_base, out_stream->time_base);
        packet->pos = -1;
        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) return ret;
    }
    return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
}

static int send_audio_frame(AVFormatContext *output,
                            AVCodecContext *encoder,
                            AVStream *out_stream,
                            AVPacket *packet,
                            AVFrame *frame)
{
    int ret = avcodec_send_frame(encoder, frame);
    if (ret < 0) return ret;
    return drain_audio_encoder(output, encoder, out_stream, packet);
}

static int write_resampled_to_fifo(SwrContext *swr,
                                   AVAudioFifo *fifo,
                                   const AVCodecContext *encoder,
                                   const AVFrame *input,
                                   int decoder_rate)
{
    AVFrame *converted = NULL;
    int out_capacity;
    int converted_count;
    int ret = 0;

    out_capacity = (int)av_rescale_rnd(
        swr_get_delay(swr, decoder_rate) + input->nb_samples,
        encoder->sample_rate, decoder_rate, AV_ROUND_UP);
    if (out_capacity <= 0) return 0;

    converted = av_frame_alloc();
    if (!converted) return AVERROR(ENOMEM);
    converted->format = encoder->sample_fmt;
    converted->sample_rate = encoder->sample_rate;
    converted->nb_samples = out_capacity;
    ret = av_channel_layout_copy(&converted->ch_layout, &encoder->ch_layout);
    if (ret < 0) goto end;
    ret = av_frame_get_buffer(converted, 0);
    if (ret < 0) goto end;

    converted_count = swr_convert(swr, converted->data, out_capacity,
                                  (const uint8_t **)input->extended_data,
                                  input->nb_samples);
    if (converted_count < 0) {
        ret = converted_count;
        goto end;
    }
    if (converted_count == 0) goto end;

    ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + converted_count);
    if (ret < 0) goto end;
    if (av_audio_fifo_write(fifo, (void **)converted->extended_data, converted_count)
        < converted_count) {
        ret = AVERROR(EIO);
        goto end;
    }
    ret = 0;

end:
    av_frame_free(&converted);
    return ret;
}

static int flush_resampler_to_fifo(SwrContext *swr,
                                   AVAudioFifo *fifo,
                                   const AVCodecContext *encoder,
                                   int decoder_rate)
{
    int ret = 0;
    while (swr_get_delay(swr, decoder_rate) > 0) {
        AVFrame *converted = NULL;
        int out_capacity = (int)av_rescale_rnd(
            swr_get_delay(swr, decoder_rate),
            encoder->sample_rate, decoder_rate, AV_ROUND_UP);
        int converted_count;
        if (out_capacity <= 0) break;

        converted = av_frame_alloc();
        if (!converted) return AVERROR(ENOMEM);
        converted->format = encoder->sample_fmt;
        converted->sample_rate = encoder->sample_rate;
        converted->nb_samples = out_capacity;
        ret = av_channel_layout_copy(&converted->ch_layout, &encoder->ch_layout);
        if (ret < 0) {
            av_frame_free(&converted);
            return ret;
        }
        ret = av_frame_get_buffer(converted, 0);
        if (ret < 0) {
            av_frame_free(&converted);
            return ret;
        }
        converted_count = swr_convert(swr, converted->data, out_capacity, NULL, 0);
        if (converted_count < 0) {
            av_frame_free(&converted);
            return converted_count;
        }
        if (converted_count == 0) {
            av_frame_free(&converted);
            break;
        }
        ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + converted_count);
        if (ret < 0) {
            av_frame_free(&converted);
            return ret;
        }
        if (av_audio_fifo_write(fifo, (void **)converted->extended_data, converted_count)
            < converted_count) {
            av_frame_free(&converted);
            return AVERROR(EIO);
        }
        av_frame_free(&converted);
    }
    return 0;
}

static int encode_audio_fifo(AVAudioFifo *fifo,
                             AVFormatContext *output,
                             AVCodecContext *encoder,
                             AVStream *out_stream,
                             AVPacket *packet,
                             int flush,
                             int64_t *next_pts)
{
    int frame_size = encoder->frame_size > 0 ? encoder->frame_size : 1024;
    int ret = 0;

    while (av_audio_fifo_size(fifo) >= frame_size ||
           (flush && av_audio_fifo_size(fifo) > 0)) {
        AVFrame *frame = NULL;
        int available = av_audio_fifo_size(fifo);
        int read_samples = FFMIN(available, frame_size);
        int send_samples = read_samples;
        int can_short = (encoder->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ||
                        (encoder->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME) ||
                        encoder->frame_size <= 0;

        if (read_samples < frame_size && !can_short)
            send_samples = frame_size;

        frame = av_frame_alloc();
        if (!frame) return AVERROR(ENOMEM);
        frame->format = encoder->sample_fmt;
        frame->sample_rate = encoder->sample_rate;
        frame->nb_samples = send_samples;
        frame->pts = *next_pts;
        ret = av_channel_layout_copy(&frame->ch_layout, &encoder->ch_layout);
        if (ret < 0) {
            av_frame_free(&frame);
            return ret;
        }
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            av_frame_free(&frame);
            return ret;
        }
        av_samples_set_silence(frame->extended_data, 0, send_samples,
                               encoder->ch_layout.nb_channels, encoder->sample_fmt);
        if (av_audio_fifo_read(fifo, (void **)frame->extended_data, read_samples)
            < read_samples) {
            av_frame_free(&frame);
            return AVERROR(EIO);
        }

        ret = send_audio_frame(output, encoder, out_stream, packet, frame);
        av_frame_free(&frame);
        if (ret < 0) return ret;
        *next_pts += send_samples;
    }
    return ret;
}

static int transcode_audio(const RunnerOptions *options)
{
    AVFormatContext *input = NULL;
    AVFormatContext *output = NULL;
    AVCodecContext *decoder = NULL;
    AVCodecContext *encoder = NULL;
    AVStream *in_stream;
    AVStream *out_stream = NULL;
    const AVCodec *decoder_codec;
    const AVCodec *encoder_codec;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    AVAudioFifo *fifo = NULL;
    SwrContext *swr = NULL;
    enum AVCodecID encoder_id;
    const char *muxer;
    int bitrate_kbps = options->bitrate_kbps;
    double last_progress = -1;
    int64_t next_pts = 0;
    int header_written = 0;
    int decoder_rate = 0;
    int ret;

    if (!strcmp(options->transcode_format, "m4a")) {
        encoder_id = AV_CODEC_ID_AAC;
        muxer = "ipod";
        if (!bitrate_kbps) bitrate_kbps = 192;
        if (bitrate_kbps != 128 && bitrate_kbps != 192 && bitrate_kbps != 256)
            return AVERROR(EINVAL);
    } else if (!strcmp(options->transcode_format, "wav")) {
        encoder_id = AV_CODEC_ID_PCM_S16LE;
        muxer = "wav";
    } else {
        return AVERROR(EINVAL);
    }

    ret = avformat_open_input(&input, options->input_path, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(input, NULL);
    if (ret < 0) goto end;
    if ((unsigned int)options->audio_stream_index >= input->nb_streams) {
        ret = AVERROR(EINVAL);
        goto end;
    }
    in_stream = input->streams[options->audio_stream_index];
    if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO ||
        !transcode_source_supported(in_stream->codecpar->codec_id)) {
        ret = AVERROR(ENOSYS);
        goto end;
    }

    decoder_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!decoder_codec) {
        ret = AVERROR_DECODER_NOT_FOUND;
        goto end;
    }
    decoder = avcodec_alloc_context3(decoder_codec);
    if (!decoder) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ret = avcodec_parameters_to_context(decoder, in_stream->codecpar);
    if (ret < 0) goto end;
    decoder->pkt_timebase = in_stream->time_base;
    decoder->thread_count = 1;
    decoder->thread_type = 0;
    ret = avcodec_open2(decoder, decoder_codec, NULL);
    if (ret < 0) goto end;

    encoder_codec = avcodec_find_encoder(encoder_id);
    if (!encoder_codec) {
        ret = AVERROR_ENCODER_NOT_FOUND;
        goto end;
    }
    ret = avformat_alloc_output_context2(&output, NULL, muxer, options->output_path);
    if (ret < 0 || !output) {
        if (ret >= 0) ret = AVERROR(EINVAL);
        goto end;
    }
    out_stream = avformat_new_stream(output, NULL);
    if (!out_stream) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    encoder = avcodec_alloc_context3(encoder_codec);
    if (!encoder) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    encoder->codec_type = AVMEDIA_TYPE_AUDIO;
    encoder->sample_rate = !strcmp(options->transcode_format, "wav")
        ? (decoder->sample_rate > 0 ? decoder->sample_rate : 48000)
        : choose_audio_sample_rate(encoder_codec, decoder->sample_rate);
    encoder->sample_fmt = choose_audio_sample_format(
        encoder_codec,
        encoder_id == AV_CODEC_ID_PCM_S16LE ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_FLTP);
    encoder->time_base = (AVRational){1, encoder->sample_rate};
    encoder->thread_count = 1;
    encoder->thread_type = 0;
    if (encoder_id == AV_CODEC_ID_AAC)
        encoder->bit_rate = (int64_t)bitrate_kbps * 1000;

    if (decoder->ch_layout.nb_channels > 0)
        ret = av_channel_layout_copy(&encoder->ch_layout, &decoder->ch_layout);
    else {
        av_channel_layout_default(&encoder->ch_layout, 2);
        ret = 0;
    }
    if (ret < 0) goto end;

    if (output->oformat->flags & AVFMT_GLOBALHEADER)
        encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ret = avcodec_open2(encoder, encoder_codec, NULL);
    if (ret < 0) goto end;
    ret = avcodec_parameters_from_context(out_stream->codecpar, encoder);
    if (ret < 0) goto end;
    out_stream->time_base = encoder->time_base;
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
    frame = av_frame_alloc();
    fifo = av_audio_fifo_alloc(encoder->sample_fmt,
                               encoder->ch_layout.nb_channels, 1);
    if (!packet || !frame || !fifo) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while ((ret = av_read_frame(input, packet)) >= 0) {
        if (packet->stream_index != options->audio_stream_index) {
            av_packet_unref(packet);
            continue;
        }
        emit_progress(packet, in_stream, input->duration, &last_progress);
        ret = avcodec_send_packet(decoder, packet);
        av_packet_unref(packet);
        if (ret < 0) goto end;

        while ((ret = avcodec_receive_frame(decoder, frame)) >= 0) {
            if (!swr) {
                decoder_rate = frame->sample_rate > 0 ? frame->sample_rate : decoder->sample_rate;
                if (decoder_rate <= 0) decoder_rate = encoder->sample_rate;
                ret = swr_alloc_set_opts2(&swr,
                                          &encoder->ch_layout, encoder->sample_fmt,
                                          encoder->sample_rate,
                                          &frame->ch_layout,
                                          (enum AVSampleFormat)frame->format,
                                          decoder_rate, 0, NULL);
                if (ret < 0) goto end;
                ret = swr_init(swr);
                if (ret < 0) goto end;
            }
            ret = write_resampled_to_fifo(swr, fifo, encoder, frame, decoder_rate);
            av_frame_unref(frame);
            if (ret < 0) goto end;
            ret = encode_audio_fifo(fifo, output, encoder, out_stream, packet,
                                    0, &next_pts);
            if (ret < 0) goto end;
        }
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) goto end;
        ret = 0;
    }
    if (ret != AVERROR_EOF) goto end;
    ret = 0;

    ret = avcodec_send_packet(decoder, NULL);
    if (ret < 0 && ret != AVERROR_EOF) goto end;
    while ((ret = avcodec_receive_frame(decoder, frame)) >= 0) {
        if (!swr) {
            decoder_rate = frame->sample_rate > 0 ? frame->sample_rate : decoder->sample_rate;
            if (decoder_rate <= 0) decoder_rate = encoder->sample_rate;
            ret = swr_alloc_set_opts2(&swr,
                                      &encoder->ch_layout, encoder->sample_fmt,
                                      encoder->sample_rate,
                                      &frame->ch_layout,
                                      (enum AVSampleFormat)frame->format,
                                      decoder_rate, 0, NULL);
            if (ret < 0) goto end;
            ret = swr_init(swr);
            if (ret < 0) goto end;
        }
        ret = write_resampled_to_fifo(swr, fifo, encoder, frame, decoder_rate);
        av_frame_unref(frame);
        if (ret < 0) goto end;
    }
    if (ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) goto end;
    ret = 0;

    if (swr) {
        ret = flush_resampler_to_fifo(swr, fifo, encoder, decoder_rate);
        if (ret < 0) goto end;
    }
    ret = encode_audio_fifo(fifo, output, encoder, out_stream, packet, 1, &next_pts);
    if (ret < 0) goto end;
    ret = send_audio_frame(output, encoder, out_stream, packet, NULL);
    if (ret < 0) goto end;

    ret = av_write_trailer(output);
    if (ret < 0) goto end;
    header_written = 0;
    printf(PROGRESS_PREFIX " 1.000000\\n");
    printf("video-audio-extractor: transcode stream=%d codec=%s format=%s bitrate=%d\\n",
           options->audio_stream_index, avcodec_get_name(in_stream->codecpar->codec_id),
           options->transcode_format, bitrate_kbps);
    fflush(stdout);

end:
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Audio transcode failed: %s\\n", av_err2str(ret));
    if (ret < 0 && header_written && output) av_write_trailer(output);
    av_audio_fifo_free(fifo);
    swr_free(&swr);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    avcodec_free_context(&encoder);
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

    if (options.operation == OP_INSPECT)
        ret = inspect_media(&options);
    else if (options.operation == OP_COPY)
        ret = copy_audio(&options);
    else
        ret = transcode_audio(&options);
    if (ret < 0) {
        fprintf(stderr, "Video Audio Extractor failed: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
