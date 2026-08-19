/*
 * FFmpeg WASM Builder - Lossless Video Cutter runner.
 *
 * Uses FFmpeg public libavformat/libavcodec packet APIs only. Compressed packets
 * are copied into a new container; no decoder, encoder, filter, swscale, or
 * swresample stage is used.
 *
 * The remuxing pattern follows FFmpeg's public doc/examples/remux.c example.
 *
 * The upstream remux example carries the following MIT license notice:
 * Copyright (c) 2013 Stefano Sabatini
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>

#define PROGRESS_PREFIX "__FFMPEG_WASM_PROGRESS__"
#define RUNNER_VERSION "1.5.0"

typedef struct RunnerOptions {
    const char *input_path;
    const char *output_path;
    double start_seconds;
    double end_seconds;
    int no_audio;
} RunnerOptions;

static void print_usage(const char *program)
{
    fprintf(stderr,
        "FFmpeg WASM Lossless Video Cutter %s\n"
        "Usage:\n"
        "  %s --input INPUT --output OUTPUT [options]\n\n"
        "Options:\n"
        "  --start SECONDS       Requested start time (default 0)\n"
        "  --end SECONDS         Requested end time; 0 means EOF\n"
        "  --no-audio            Drop the audio stream\n"
        "  --version             Print runner/FFmpeg version\n"
        "  --help                Show this help\n\n"
        "The start is keyframe aligned. For inter-frame video the actual cut may\n"
        "begin slightly before the requested time so the first frame is decodable.\n",
        RUNNER_VERSION, program);
}

static int parse_seconds(const char *value, double *out)
{
    char *end = NULL;
    double parsed;

    errno = 0;
    parsed = strtod(value, &end);
    if (errno || !end || *end != '\0' || !isfinite(parsed) || parsed < 0.0 || parsed > 315360000.0)
        return AVERROR(EINVAL);
    *out = parsed;
    return 0;
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
            printf("lossless-video-cutter %s / FFmpeg %s\n", RUNNER_VERSION, av_version_info());
            return 1;
        }
        if (!strcmp(arg, "--no-audio")) {
            options->no_audio = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg);
            return AVERROR(EINVAL);
        }
        value = argv[++i];
        if (!strcmp(arg, "--input")) options->input_path = value;
        else if (!strcmp(arg, "--output")) options->output_path = value;
        else if (!strcmp(arg, "--start")) {
            if (parse_seconds(value, &options->start_seconds) < 0) goto invalid;
        } else if (!strcmp(arg, "--end")) {
            if (parse_seconds(value, &options->end_seconds) < 0) goto invalid;
        } else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return AVERROR(EINVAL);
        }
        continue;
invalid:
        fprintf(stderr, "Invalid value for %s: %s\n", arg, value);
        return AVERROR(EINVAL);
    }

    if (!options->input_path || !options->output_path) {
        fprintf(stderr, "--input and --output are required.\n");
        return AVERROR(EINVAL);
    }
    if (options->end_seconds > 0.0 && options->end_seconds <= options->start_seconds) {
        fprintf(stderr, "--end must be greater than --start.\n");
        return AVERROR(EINVAL);
    }
    return 0;
}

static int64_t seconds_to_us(double seconds)
{
    return (int64_t)llround(seconds * (double)AV_TIME_BASE);
}

static int64_t packet_time_us(const AVPacket *packet, const AVStream *stream)
{
    int64_t ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (ts == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;
    return av_rescale_q(ts, stream->time_base, AV_TIME_BASE_Q);
}

static int add_output_stream(AVFormatContext *output,
                             AVStream *input_stream,
                             int input_index,
                             int *stream_map)
{
    AVStream *out_stream = avformat_new_stream(output, NULL);
    int ret;

    if (!out_stream)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_copy(out_stream->codecpar, input_stream->codecpar);
    if (ret < 0)
        return ret;

    out_stream->codecpar->codec_tag = 0;
    out_stream->time_base = input_stream->time_base;
    out_stream->avg_frame_rate = input_stream->avg_frame_rate;
    out_stream->disposition = input_stream->disposition;
    av_dict_copy(&out_stream->metadata, input_stream->metadata, 0);
    stream_map[input_index] = out_stream->index;
    return 0;
}

static int seek_to_absolute_time(AVFormatContext *input,
                                 AVStream *video_stream,
                                 int video_index,
                                 int64_t target_abs_us)
{
    int64_t target_ts;
    int ret;

    target_ts = av_rescale_q(target_abs_us, AV_TIME_BASE_Q, video_stream->time_base);
    ret = av_seek_frame(input, video_index, target_ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        av_log(NULL, AV_LOG_WARNING,
               "Keyframe seek failed (%s); retrying with the format-level seek API.\n",
               av_err2str(ret));
        ret = avformat_seek_file(input, -1, INT64_MIN, target_abs_us, target_abs_us, AVSEEK_FLAG_BACKWARD);
    }
    if (ret >= 0)
        avformat_flush(input);
    return ret;
}

static void emit_progress(int64_t packet_abs_us,
                          int64_t requested_start_abs_us,
                          int64_t requested_end_abs_us,
                          int64_t duration_us,
                          double *last_progress)
{
    double progress;
    int64_t denom;

    if (packet_abs_us == AV_NOPTS_VALUE)
        return;
    if (requested_end_abs_us > requested_start_abs_us)
        denom = requested_end_abs_us - requested_start_abs_us;
    else if (duration_us > 0)
        denom = duration_us;
    else
        return;

    progress = (double)(packet_abs_us - requested_start_abs_us) / (double)denom;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    if (progress >= *last_progress + 0.005 || progress >= 1.0) {
        printf(PROGRESS_PREFIX " %.6f\n", progress);
        fflush(stdout);
        *last_progress = progress;
    }
}

static int run_cut(const RunnerOptions *options)
{
    AVFormatContext *input = NULL;
    AVFormatContext *output = NULL;
    AVPacket *packet = NULL;
    AVDictionary *mux_options = NULL;
    int *stream_map = NULL;
    int video_index = -1;
    int audio_index = -1;
    int64_t input_origin_us = 0;
    int64_t requested_start_us = seconds_to_us(options->start_seconds);
    int64_t requested_end_us = options->end_seconds > 0.0 ? seconds_to_us(options->end_seconds) : 0;
    int64_t requested_start_abs_us;
    int64_t requested_end_abs_us = 0;
    int64_t actual_start_abs_us = AV_NOPTS_VALUE;
    int64_t actual_start_rel_us = 0;
    double last_progress = -1.0;
    int header_written = 0;
    int packets_written = 0;
    int ret = 0;
    unsigned int i;

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

    video_index = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index < 0) {
        ret = video_index;
        av_log(NULL, AV_LOG_ERROR, "No video stream was found.\n");
        goto end;
    }
    if (!options->no_audio) {
        int found_audio = av_find_best_stream(input, AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
        if (found_audio >= 0)
            audio_index = found_audio;
    }

    if (input->start_time != AV_NOPTS_VALUE)
        input_origin_us = input->start_time;
    requested_start_abs_us = input_origin_us + requested_start_us;
    if (requested_end_us > 0)
        requested_end_abs_us = input_origin_us + requested_end_us;

    /*
     * First locate the keyframe that will become the actual start.  We then
     * seek back to that exact point before muxing.  The second seek matters:
     * audio packets can be interleaved before the first video packet returned
     * by the demuxer, and a one-pass scan would silently drop those packets.
     */
    if (requested_start_us > 0) {
        ret = seek_to_absolute_time(input, input->streams[video_index], video_index,
                                    requested_start_abs_us);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not seek to the requested start: %s\n", av_err2str(ret));
            goto end;
        }
    }

    packet = av_packet_alloc();
    if (!packet) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    while ((ret = av_read_frame(input, packet)) >= 0) {
        if (packet->stream_index == video_index &&
            (packet->flags & AV_PKT_FLAG_KEY)) {
            int64_t packet_abs_us = packet_time_us(packet, input->streams[video_index]);
            if (packet_abs_us != AV_NOPTS_VALUE) {
                actual_start_abs_us = packet_abs_us;
                av_packet_unref(packet);
                break;
            }
        }
        av_packet_unref(packet);
    }
    if (ret == AVERROR_EOF || actual_start_abs_us == AV_NOPTS_VALUE) {
        av_log(NULL, AV_LOG_ERROR, "No decodable video keyframe was found for the selected range.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }
    if (ret < 0)
        goto end;

    actual_start_rel_us = actual_start_abs_us - input_origin_us;
    if (actual_start_rel_us < 0)
        actual_start_rel_us = 0;
    if (requested_end_abs_us > 0 && actual_start_abs_us >= requested_end_abs_us) {
        av_log(NULL, AV_LOG_ERROR,
               "No decodable keyframe exists before the requested end time.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }
    printf("lossless-cut: requested-start=%.6f actual-start=%.6f keyframe-aligned=yes\n",
           options->start_seconds,
           (double)actual_start_rel_us / AV_TIME_BASE);
    fflush(stdout);

    ret = seek_to_absolute_time(input, input->streams[video_index], video_index,
                                actual_start_abs_us);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not return to the selected keyframe: %s\n", av_err2str(ret));
        goto end;
    }

    ret = avformat_alloc_output_context2(&output, NULL, NULL, options->output_path);
    if (ret < 0 || !output) {
        if (ret >= 0) ret = AVERROR(EINVAL);
        av_log(NULL, AV_LOG_ERROR,
               "Could not choose an output container from '%s': %s\n",
               options->output_path, av_err2str(ret));
        goto end;
    }

    stream_map = av_calloc(input->nb_streams, sizeof(*stream_map));
    if (!stream_map) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    for (i = 0; i < input->nb_streams; i++)
        stream_map[i] = -1;

    ret = add_output_stream(output, input->streams[video_index], video_index, stream_map);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not create the output video stream: %s\n", av_err2str(ret));
        goto end;
    }
    if (audio_index >= 0) {
        ret = add_output_stream(output, input->streams[audio_index], audio_index, stream_map);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not create the output audio stream: %s\n", av_err2str(ret));
            goto end;
        }
    }
    av_dict_copy(&output->metadata, input->metadata, 0);
    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_ZERO;

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output->pb, options->output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output: %s\n", av_err2str(ret));
            goto end;
        }
    }

    ret = avformat_write_header(output, &mux_options);
    av_dict_free(&mux_options);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Output container rejected the copied streams: %s\n",
               av_err2str(ret));
        goto end;
    }
    header_written = 1;

    while ((ret = av_read_frame(input, packet)) >= 0) {
        AVStream *in_stream;
        AVStream *out_stream;
        int64_t packet_abs_us;
        int64_t anchor_ts;
        int mapped_index;
        int should_break = 0;

        if (packet->stream_index < 0 || (unsigned int)packet->stream_index >= input->nb_streams) {
            av_packet_unref(packet);
            continue;
        }
        mapped_index = stream_map[packet->stream_index];
        if (mapped_index < 0) {
            av_packet_unref(packet);
            continue;
        }

        in_stream = input->streams[packet->stream_index];
        packet_abs_us = packet_time_us(packet, in_stream);

        if (packet_abs_us != AV_NOPTS_VALUE && packet_abs_us < actual_start_abs_us) {
            av_packet_unref(packet);
            continue;
        }
        if (requested_end_abs_us > 0 && packet_abs_us != AV_NOPTS_VALUE && packet_abs_us >= requested_end_abs_us) {
            if (packet->stream_index == video_index)
                should_break = 1;
            av_packet_unref(packet);
            if (should_break)
                break;
            continue;
        }

        out_stream = output->streams[mapped_index];
        anchor_ts = av_rescale_q(actual_start_abs_us, AV_TIME_BASE_Q, in_stream->time_base);
        if (packet->pts != AV_NOPTS_VALUE)
            packet->pts -= anchor_ts;
        if (packet->dts != AV_NOPTS_VALUE)
            packet->dts -= anchor_ts;
        av_packet_rescale_ts(packet, in_stream->time_base, out_stream->time_base);
        packet->stream_index = mapped_index;
        packet->pos = -1;

        ret = av_interleaved_write_frame(output, packet);
        av_packet_unref(packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not mux copied packet: %s\n", av_err2str(ret));
            goto end;
        }
        packets_written++;
        emit_progress(packet_abs_us, requested_start_abs_us, requested_end_abs_us,
                      input->duration, &last_progress);
    }

    if (ret == AVERROR_EOF)
        ret = 0;
    if (ret < 0)
        goto end;
    if (packets_written <= 0) {
        av_log(NULL, AV_LOG_ERROR, "The selected range produced no output packets.\n");
        ret = AVERROR(EINVAL);
        goto end;
    }

    ret = av_write_trailer(output);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not finalize output: %s\n", av_err2str(ret));
        goto end;
    }
    header_written = 0;
    printf(PROGRESS_PREFIX " 1.000000\n");
    printf("lossless-cut: packets=%d requested-end=%.6f\n",
           packets_written, options->end_seconds);
    fflush(stdout);

end:
    if (ret < 0 && header_written && output)
        av_write_trailer(output);
    av_packet_free(&packet);
    av_freep(&stream_map);
    av_dict_free(&mux_options);
    if (input)
        avformat_close_input(&input);
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

    av_log_set_level(AV_LOG_INFO);
    parsed = parse_options(argc, argv, &options);
    if (parsed > 0)
        return 0;
    if (parsed < 0)
        return 2;

    ret = run_cut(&options);
    if (ret < 0) {
        fprintf(stderr, "Lossless Video Cutter failed: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
