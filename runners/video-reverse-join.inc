/*
 * Experimental join mode for video-reverse and ping-pong chunks.
 * All inputs MUST be encoded by the same profile at the same geometry,
 * fps and codec parameters. MP4 is remuxed, not byte-concatenated.
 * Public libavformat/libavcodec API only. No FFmpeg CLI.
 */
#define REVERSE_JOIN_MAX_CHUNKS 16

static int reverse_join_compatible(const AVCodecParameters *first,
                                   const AVCodecParameters *next)
{
    if (!first || !next || first->codec_id != AV_CODEC_ID_H264 ||
        next->codec_id != AV_CODEC_ID_H264 ||
        first->codec_type != AVMEDIA_TYPE_VIDEO ||
        next->codec_type != AVMEDIA_TYPE_VIDEO ||
        first->width != next->width || first->height != next->height ||
        first->format != next->format ||
        first->profile != next->profile ||
        first->level != next->level ||
        first->sample_aspect_ratio.num != next->sample_aspect_ratio.num ||
        first->sample_aspect_ratio.den != next->sample_aspect_ratio.den ||
        first->extradata_size <= 0 ||
        first->extradata_size != next->extradata_size)
        return 0;

    return memcmp(first->extradata, next->extradata,
                  (size_t)first->extradata_size) == 0;
}

/*
 * CLI:
 * --concat --input /chunk-last.mp4 --input /chunk-first.mp4
 *          [--input more.mp4 ...] --output /joined.mp4
 * Caller supplies the exact presentation order: reverse-only chunks in
 * reversed chronology or ping-pong forward chunks followed by return chunks.
 * At most 16 chunks, with an exact expected total frame count.
 */
static int reverse_join_chunks(int argc, char **argv)
{
    const char *inputs[REVERSE_JOIN_MAX_CHUNKS] = {0};
    const char *output = NULL;
    int n = 0, i, ret = 0;
    int expected_frames = 0, written_total = 0;
    int64_t presentation_end = 0;
    int64_t previous_dts = INT64_MIN;
    AVFormatContext *ofmt = NULL, *ifmt = NULL;
    AVStream *ostream = NULL;
    AVCodecParameters *reference = NULL;
    AVPacket *pkt = NULL;

    for (i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--input") && i + 1 < argc) {
            if (n >= REVERSE_JOIN_MAX_CHUNKS) {
                fprintf(stderr, "At most %d reverse chunks can be joined.\n",
                        REVERSE_JOIN_MAX_CHUNKS);
                return 2;
            }
            inputs[n++] = argv[++i];
        } else if (!strcmp(argv[i], "--output") && i + 1 < argc) {
            if (output) return 2;
            output = argv[++i];
        } else if (!strcmp(argv[i], "--expected-frames") && i + 1 < argc) {
            char *end = NULL;
            long count;
            if (expected_frames) return 2;
            errno = 0;
            count = strtol(argv[++i], &end, 10);
            if (errno || !end || *end || count < 2 || count > 960) return 2;
            expected_frames = (int)count;
        } else {
            fprintf(stderr, "Invalid reverse-concat argument: %s\n", argv[i]);
            return 2;
        }
    }
    if (n < 2 || !output || !*output) {
        fprintf(stderr, "Reverse concat requires 2..16 --input paths and one --output path.\n");
        return 2;
    }
    for (i = 0; i < n; ++i) {
        int j;
        if (!inputs[i] || !*inputs[i] || !strcmp(inputs[i], output)) return 2;
        for (j = 0; j < i; ++j)
            if (!strcmp(inputs[i], inputs[j])) return 2;
    }

    reference = avcodec_parameters_alloc();
    pkt = av_packet_alloc();
    if (!reference || !pkt) { ret = AVERROR(ENOMEM); goto done; }

    av_log(NULL, AV_LOG_INFO, "Reverse MP4 join: %d chunks, no audio\n", n);
    printf(PROGRESS_PREFIX " 0.000000\n");
    fflush(stdout);
    for (i = 0; i < n; ++i) {
        int stream_index, written = 0, audio_index;
        int64_t source_start = 0, chunk_end = presentation_end;
        AVRational source_tb, output_tb;

        ret = avformat_open_input(&ifmt, inputs[i], NULL, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open reversed MP4 chunk %d\n", i);
            goto done;
        }
        ret = avformat_find_stream_info(ifmt, NULL);
        if (ret < 0) goto done;
        stream_index = av_find_best_stream(ifmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (stream_index < 0) { ret = stream_index; goto done; }
        audio_index = av_find_best_stream(ifmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (audio_index >= 0) {
            av_log(NULL, AV_LOG_ERROR, "Audio is not yet supported in reverse concat.\n");
            ret = AVERROR(EINVAL);
            goto done;
        }
        AVStream *istream = ifmt->streams[stream_index];
        if (i == 0) {
            ret = avcodec_parameters_copy(reference, istream->codecpar);
            if (ret < 0) goto done;
            ret = avformat_alloc_output_context2(&ofmt, NULL, "mp4", output);
            if (ret < 0) goto done;
            if (!ofmt) { ret = AVERROR(ENOMEM); goto done; }
            ostream = avformat_new_stream(ofmt, NULL);
            if (!ostream) { ret = AVERROR(ENOMEM); goto done; }
            ret = avcodec_parameters_copy(ostream->codecpar, reference);
            if (ret < 0) goto done;
            ostream->codecpar->codec_tag = 0;
            ostream->time_base = istream->time_base;
            if (ostream->time_base.num <= 0 || ostream->time_base.den <= 0) {
                ret = AVERROR(EINVAL);
                goto done;
            }
            if (!(ofmt->oformat->flags & AVFMT_NOFILE)) {
                ret = avio_open(&ofmt->pb, output, AVIO_FLAG_WRITE);
                if (ret < 0) goto done;
            }
            ret = avformat_write_header(ofmt, NULL);
            if (ret < 0) goto done;
        } else if (!reverse_join_compatible(reference, istream->codecpar)) {
            av_log(NULL, AV_LOG_ERROR,
                   "Reverse chunks have incompatible codec configuration/geometry.\n");
            ret = AVERROR(EINVAL);
            goto done;
        }
        source_tb = istream->time_base;
        output_tb = ostream->time_base;
        if (istream->start_time != AV_NOPTS_VALUE)
            source_start = av_rescale_q(istream->start_time, source_tb, output_tb);

        while ((ret = av_read_frame(ifmt, pkt)) >= 0) {
            int64_t end;
            if (pkt->stream_index != stream_index) {
                av_packet_unref(pkt);
                continue;
            }
            if (pkt->pts == AV_NOPTS_VALUE || pkt->dts == AV_NOPTS_VALUE) {
                av_log(NULL, AV_LOG_ERROR,
                       "Reverse concat cannot use packets without PTS/DTS.\n");
                ret = AVERROR_INVALIDDATA;
                av_packet_unref(pkt);
                goto done;
            }
            av_packet_rescale_ts(pkt, source_tb, output_tb);
            if (pkt->duration <= 0) {
                const AVRational fps = istream->avg_frame_rate;
                if (fps.num <= 0 || fps.den <= 0) {
                    ret = AVERROR_INVALIDDATA;
                    av_packet_unref(pkt);
                    goto done;
                }
                pkt->duration = av_rescale_q(1, av_inv_q(fps), output_tb);
                if (pkt->duration <= 0) {
                    ret = AVERROR_INVALIDDATA;
                    av_packet_unref(pkt);
                    goto done;
                }
            }
            pkt->pts = pkt->pts - source_start + presentation_end;
            pkt->dts = pkt->dts - source_start + presentation_end;
            if (pkt->dts <= previous_dts) {
                av_log(NULL, AV_LOG_ERROR,
                       "Non-monotonic reverse chunk DTS at boundary %d.\n", i);
                ret = AVERROR_INVALIDDATA;
                av_packet_unref(pkt);
                goto done;
            }
            previous_dts = pkt->dts;
            end = pkt->pts + pkt->duration;
            if (end > chunk_end) chunk_end = end;
            pkt->stream_index = ostream->index;
            pkt->pos = -1;
            ret = av_interleaved_write_frame(ofmt, pkt);
            av_packet_unref(pkt);
            if (ret < 0) goto done;
            ++written;
            ++written_total;
            if (written > 150) {
                av_log(NULL, AV_LOG_ERROR,
                       "Reverse chunk has more than 150 video packets.\n");
                ret = AVERROR(EINVAL);
                goto done;
            }
        }
        if (ret != AVERROR_EOF) goto done;
        if (!written || chunk_end <= presentation_end) {
            ret = AVERROR_INVALIDDATA;
            goto done;
        }
        presentation_end = chunk_end;
        avformat_close_input(&ifmt);
        printf(PROGRESS_PREFIX " %.6f\n", (i + 1.0) / n);
        fflush(stdout);
    }

    if (expected_frames && expected_frames != written_total) {
        av_log(NULL, AV_LOG_ERROR,
               "Reverse joined frame mismatch: expected=%d actual=%d.\n",
               expected_frames, written_total);
        ret = AVERROR_INVALIDDATA;
        goto done;
    }
    ret = av_write_trailer(ofmt);
    if (ret >= 0)
        av_log(NULL, AV_LOG_INFO, "Reverse MP4 joined: %d chunks.\n", n);

done:
    av_packet_free(&pkt);
    avformat_close_input(&ifmt);
    if (ofmt) {
        if (!(ofmt->oformat->flags & AVFMT_NOFILE) && ofmt->pb)
            avio_closep(&ofmt->pb);
        avformat_free_context(ofmt);
    }
    avcodec_parameters_free(&reference);
    if (ret < 0) {
        if (output) remove(output);
        av_log(NULL, AV_LOG_ERROR, "Reverse MP4 concat failed: %s\n", av_err2str(ret));
    }
    return ret < 0 ? 1 : 0;
}
