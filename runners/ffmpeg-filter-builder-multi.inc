/*
 * Multi-input / complex-graph execution path for FFmpeg Filter Builder.
 * Included by runners/ffmpeg-filter-builder.c after the legacy single-input
 * implementation so the proven v1.9.8 path remains unchanged.
 */

#define MULTI_MAX_INPUTS 8

typedef struct MultiStream {
    int present;
    int stream_index;
    enum AVMediaType type;
    AVCodecContext *dec_ctx;
    AVFrame *dec_frame;
    AVFilterContext *src_ctx;
    AVRational filter_time_base;
    int source_eof;
} MultiStream;

typedef struct MultiInput {
    const char *path;
    const char *kind;
    AVFormatContext *fmt_ctx;
    MultiStream video;
    MultiStream audio;
    AVPacket *packet;
    int demux_eof;
} MultiInput;

typedef struct MultiOutput {
    AVCodecContext *enc_ctx;
    AVStream *stream;
    AVFilterContext *sink_ctx;
    AVFrame *frame;
    AVPacket *packet;
    enum AVMediaType type;
} MultiOutput;

typedef struct MultiContext {
    RunnerOptions *options;
    MultiInput inputs[MULTI_MAX_INPUTS];
    int input_count;
    AVFilterGraph *graph;
    MultiOutput video;
    MultiOutput audio;
    AVFormatContext *ofmt_ctx;
    int have_video;
    int have_audio;
    int main_input_index;
    int64_t progress_duration_us;
    double last_progress;
} MultiContext;

static void multi_reset_stream(MultiStream *stream, enum AVMediaType type)
{
    memset(stream, 0, sizeof(*stream));
    stream->stream_index = -1;
    stream->type = type;
}

static void multi_reset_output(MultiOutput *output, enum AVMediaType type)
{
    memset(output, 0, sizeof(*output));
    output->type = type;
}

static int multi_label_used(const char *graph, int input_index, char media)
{
    char token[32];
    if (!graph || !*graph)
        return 0;
    snprintf(token, sizeof(token), "[%d:%c]", input_index, media);
    return strstr(graph, token) != NULL;
}

static const char *multi_strip_map_label(const char *value, char *buffer, size_t buffer_size)
{
    size_t len;
    if (!value || !*value || !buffer || buffer_size < 2)
        return NULL;
    while (*value == '[')
        value++;
    len = strlen(value);
    while (len && (value[len - 1] == ']' || value[len - 1] == '?'))
        len--;
    if (!len || len >= buffer_size)
        return NULL;
    memcpy(buffer, value, len);
    buffer[len] = '\0';
    return buffer;
}

static int multi_open_decoder(AVFormatContext *format, int stream_index, MultiStream *stream)
{
    AVStream *input_stream;
    const AVCodec *decoder;
    int ret;

    if (!format || stream_index < 0 || !stream)
        return AVERROR(EINVAL);
    input_stream = format->streams[stream_index];
    decoder = avcodec_find_decoder(input_stream->codecpar->codec_id);
    if (!decoder)
        return AVERROR_DECODER_NOT_FOUND;

    stream->dec_ctx = avcodec_alloc_context3(decoder);
    if (!stream->dec_ctx)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(stream->dec_ctx, input_stream->codecpar);
    if (ret < 0)
        return ret;
    stream->dec_ctx->pkt_timebase = input_stream->time_base;
    /*
     * Multi-input graphs keep each decoder single-threaded even in the MT
     * runtime.  Several inputs are open at the same time, while libx264 also
     * needs its own bounded pthread budget.  Reserving pthread workers for
     * every decoder can exhaust the prewarmed pool before the encoder opens.
     * The multi-input loop already interleaves decoding across inputs, so a
     * one-thread decoder is the predictable browser-safe choice here.
     */
    stream->dec_ctx->thread_count = 1;
    stream->dec_ctx->thread_type = 0;
    av_log(NULL, AV_LOG_INFO,
           "multi-input decoder: stream=%d media=%s threads=1\n",
           stream_index, stream->type == AVMEDIA_TYPE_VIDEO ? "video" : "audio");
    ret = avcodec_open2(stream->dec_ctx, decoder, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Could not open multi-input %s decoder: %s (%d)\n",
               stream->type == AVMEDIA_TYPE_VIDEO ? "video" : "audio",
               av_err2str(ret), ret);
        return ret;
    }

    stream->dec_frame = av_frame_alloc();
    if (!stream->dec_frame)
        return AVERROR(ENOMEM);
    stream->stream_index = stream_index;
    stream->filter_time_base = choose_filter_time_base(stream->type, input_stream->time_base);
    stream->present = 1;
    return 0;
}

static int multi_open_inputs(MultiContext *ctx)
{
    int i;
    int ret;
    const char *graph = ctx->options->filter_complex;

    for (i = 0; i < ctx->input_count; i++) {
        MultiInput *input = &ctx->inputs[i];
        int video_index = -1;
        int audio_index = -1;
        int need_video;
        int need_audio;

        multi_reset_stream(&input->video, AVMEDIA_TYPE_VIDEO);
        multi_reset_stream(&input->audio, AVMEDIA_TYPE_AUDIO);
        input->path = ctx->options->input_paths[i];
        input->kind = ctx->options->input_kinds[i] ? ctx->options->input_kinds[i] : "media";
        input->packet = av_packet_alloc();
        if (!input->packet)
            return AVERROR(ENOMEM);

        ret = avformat_open_input(&input->fmt_ctx, input->path, NULL, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open multi input %d: %s\n", i, input->path);
            return ret;
        }
        ret = avformat_find_stream_info(input->fmt_ctx, NULL);
        if (ret < 0)
            return ret;

        need_video = multi_label_used(graph, i, 'v');
        need_audio = multi_label_used(graph, i, 'a');
        if (need_video) {
            video_index = av_find_best_stream(input->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
            if (video_index < 0) {
                av_log(NULL, AV_LOG_ERROR, "Input %d has no required video stream\n", i);
                return video_index;
            }
            ret = multi_open_decoder(input->fmt_ctx, video_index, &input->video);
            if (ret < 0)
                return ret;
        }
        if (need_audio) {
            audio_index = av_find_best_stream(input->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
            if (audio_index < 0) {
                av_log(NULL, AV_LOG_ERROR, "Input %d has no required audio stream\n", i);
                return audio_index;
            }
            ret = multi_open_decoder(input->fmt_ctx, audio_index, &input->audio);
            if (ret < 0)
                return ret;
        }

        av_dump_format(input->fmt_ctx, 0, input->path, 0);
    }

    if (ctx->main_input_index < 0 || ctx->main_input_index >= ctx->input_count)
        ctx->main_input_index = 0;
    if (ctx->inputs[ctx->main_input_index].fmt_ctx &&
        ctx->inputs[ctx->main_input_index].fmt_ctx->duration > 0) {
        int64_t duration = ctx->inputs[ctx->main_input_index].fmt_ctx->duration;
        int64_t start = (int64_t)llround(ctx->options->start_time_seconds * AV_TIME_BASE);
        int64_t bounded = (int64_t)llround(ctx->options->duration_seconds * AV_TIME_BASE);
        ctx->progress_duration_us = bounded > 0 ? bounded : FFMAX(1, duration - start);
    }
    return 0;
}

static int multi_append_inout(AVFilterInOut **list, const char *name,
                              AVFilterContext *filter_ctx, int pad_idx)
{
    AVFilterInOut *entry = avfilter_inout_alloc();
    if (!entry)
        return AVERROR(ENOMEM);
    entry->name = av_strdup(name);
    if (!entry->name) {
        avfilter_inout_free(&entry);
        return AVERROR(ENOMEM);
    }
    entry->filter_ctx = filter_ctx;
    entry->pad_idx = pad_idx;
    entry->next = *list;
    *list = entry;
    return 0;
}

static int multi_create_video_source(MultiContext *ctx, MultiStream *stream,
                                     int input_index, AVFilterInOut **outputs)
{
    const AVFilter *buffer = avfilter_get_by_name("buffer");
    AVStream *avstream = ctx->inputs[input_index].fmt_ctx->streams[stream->stream_index];
    char args[768];
    char filter_name[32];
    char label[32];
    AVRational sar = stream->dec_ctx->sample_aspect_ratio;
    int ret;

    if (!buffer)
        return AVERROR_FILTER_NOT_FOUND;
    if (sar.num <= 0 || sar.den <= 0)
        sar = (AVRational){1, 1};
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:frame_rate=%d/%d",
             stream->dec_ctx->width, stream->dec_ctx->height, stream->dec_ctx->pix_fmt,
             stream->filter_time_base.num, stream->filter_time_base.den,
             sar.num, sar.den,
             av_guess_frame_rate(ctx->inputs[input_index].fmt_ctx, avstream, NULL).num,
             av_guess_frame_rate(ctx->inputs[input_index].fmt_ctx, avstream, NULL).den);
    snprintf(filter_name, sizeof(filter_name), "src_%d_v", input_index);
    ret = avfilter_graph_create_filter(&stream->src_ctx, buffer, filter_name, args, NULL, ctx->graph);
    if (ret < 0)
        return ret;
    snprintf(label, sizeof(label), "%d:v", input_index);
    return multi_append_inout(outputs, label, stream->src_ctx, 0);
}

static int multi_create_audio_source(MultiContext *ctx, MultiStream *stream,
                                     int input_index, AVFilterInOut **outputs)
{
    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    char args[768];
    char filter_name[32];
    char label[32];
    char layout[128];
    int ret;

    if (!abuffer)
        return AVERROR_FILTER_NOT_FOUND;
    if (stream->dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&stream->dec_ctx->ch_layout,
                                  stream->dec_ctx->ch_layout.nb_channels > 0
                                      ? stream->dec_ctx->ch_layout.nb_channels : 2);
    av_channel_layout_describe(&stream->dec_ctx->ch_layout, layout, sizeof(layout));
    snprintf(args, sizeof(args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
             stream->filter_time_base.num, stream->filter_time_base.den,
             stream->dec_ctx->sample_rate,
             av_get_sample_fmt_name(stream->dec_ctx->sample_fmt), layout);
    snprintf(filter_name, sizeof(filter_name), "src_%d_a", input_index);
    ret = avfilter_graph_create_filter(&stream->src_ctx, abuffer, filter_name, args, NULL, ctx->graph);
    if (ret < 0)
        return ret;
    snprintf(label, sizeof(label), "%d:a", input_index);
    return multi_append_inout(outputs, label, stream->src_ctx, 0);
}

static int multi_create_video_sink(MultiContext *ctx, const char *label,
                                   AVFilterInOut **inputs)
{
    const AVFilter *sink = avfilter_get_by_name("buffersink");
    enum AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
    int ret;
    if (!sink)
        return AVERROR_FILTER_NOT_FOUND;
    ctx->video.sink_ctx = avfilter_graph_alloc_filter(ctx->graph, sink, "video_out");
    if (!ctx->video.sink_ctx)
        return AVERROR(ENOMEM);
    ret = av_opt_set_array(ctx->video.sink_ctx, "pixel_formats", AV_OPT_SEARCH_CHILDREN,
                           0, 1, AV_OPT_TYPE_PIXEL_FMT, &pix_fmt);
    if (ret < 0)
        return ret;
    ret = avfilter_init_dict(ctx->video.sink_ctx, NULL);
    if (ret < 0)
        return ret;
    return multi_append_inout(inputs, label, ctx->video.sink_ctx, 0);
}

static int multi_create_audio_sink(MultiContext *ctx, const char *label,
                                   AVFilterInOut **inputs)
{
    const AVFilter *sink = avfilter_get_by_name("abuffersink");
    enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_FLTP;
    AVChannelLayout layout;
    int sample_rate = 48000;
    int ret;

    if (!sink)
        return AVERROR_FILTER_NOT_FOUND;
    ctx->audio.sink_ctx = avfilter_graph_alloc_filter(ctx->graph, sink, "audio_out");
    if (!ctx->audio.sink_ctx)
        return AVERROR(ENOMEM);
    av_channel_layout_default(&layout, 2);
    ret = av_opt_set_array(ctx->audio.sink_ctx, "sample_formats", AV_OPT_SEARCH_CHILDREN,
                           0, 1, AV_OPT_TYPE_SAMPLE_FMT, &sample_fmt);
    if (ret >= 0)
        ret = av_opt_set_array(ctx->audio.sink_ctx, "samplerates", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_INT, &sample_rate);
    if (ret >= 0)
        ret = av_opt_set_array(ctx->audio.sink_ctx, "channel_layouts", AV_OPT_SEARCH_CHILDREN,
                               0, 1, AV_OPT_TYPE_CHLAYOUT, &layout);
    av_channel_layout_uninit(&layout);
    if (ret < 0)
        return ret;
    ret = avfilter_init_dict(ctx->audio.sink_ctx, NULL);
    if (ret < 0)
        return ret;
    return multi_append_inout(inputs, label, ctx->audio.sink_ctx, 0);
}

static int multi_build_graph(MultiContext *ctx)
{
    AVFilterInOut *inputs = NULL;
    AVFilterInOut *outputs = NULL;
    char video_label[128];
    char audio_label[128];
    const char *video_map;
    const char *audio_map;
    int i;
    int ret = 0;

    ctx->graph = avfilter_graph_alloc();
    if (!ctx->graph)
        return AVERROR(ENOMEM);
    if (FFMPEG_WASM_PTHREADS)
        ctx->graph->nb_threads = 1;

    for (i = 0; i < ctx->input_count; i++) {
        if (ctx->inputs[i].video.present) {
            ret = multi_create_video_source(ctx, &ctx->inputs[i].video, i, &outputs);
            if (ret < 0) goto end;
        }
        if (ctx->inputs[i].audio.present) {
            ret = multi_create_audio_source(ctx, &ctx->inputs[i].audio, i, &outputs);
            if (ret < 0) goto end;
        }
    }

    video_map = multi_strip_map_label(ctx->options->video_map, video_label, sizeof(video_label));
    audio_map = multi_strip_map_label(ctx->options->audio_map, audio_label, sizeof(audio_label));
    if (video_map) {
        ret = multi_create_video_sink(ctx, video_map, &inputs);
        if (ret < 0) goto end;
        ctx->have_video = 1;
    }
    if (audio_map && !ctx->options->no_audio) {
        ret = multi_create_audio_sink(ctx, audio_map, &inputs);
        if (ret < 0) goto end;
        ctx->have_audio = 1;
    }
    if (!ctx->have_video)
        return AVERROR(EINVAL);

    ret = avfilter_graph_parse_ptr(ctx->graph, ctx->options->filter_complex,
                                   &inputs, &outputs, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not parse complex graph: %s\n", av_err2str(ret));
        goto end;
    }
    ret = avfilter_graph_config(ctx->graph, NULL);
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Could not configure complex graph: %s\n", av_err2str(ret));

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

static int multi_setup_video_encoder(MultiContext *ctx)
{
    const AVCodec *encoder = avcodec_find_encoder_by_name("libx264");
    AVDictionary *encoder_options = NULL;
    char crf_text[16];
    AVRational frame_rate;
    AVRational sink_time_base;
    int ret;

    if (!ctx->have_video)
        return 0;
    if (!encoder)
        return AVERROR_ENCODER_NOT_FOUND;
    ctx->video.stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
    if (!ctx->video.stream)
        return AVERROR(ENOMEM);
    ctx->video.enc_ctx = avcodec_alloc_context3(encoder);
    if (!ctx->video.enc_ctx)
        return AVERROR(ENOMEM);

    frame_rate = av_buffersink_get_frame_rate(ctx->video.sink_ctx);
    if (frame_rate.num <= 0 || frame_rate.den <= 0)
        frame_rate = (AVRational){30, 1};
    sink_time_base = av_buffersink_get_time_base(ctx->video.sink_ctx);
    if (sink_time_base.num <= 0 || sink_time_base.den <= 0)
        sink_time_base = av_inv_q(frame_rate);

    ctx->video.enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->video.enc_ctx->width = av_buffersink_get_w(ctx->video.sink_ctx);
    ctx->video.enc_ctx->height = av_buffersink_get_h(ctx->video.sink_ctx);
    ctx->video.enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->video.enc_ctx->framerate = frame_rate;
    ctx->video.enc_ctx->time_base = sink_time_base;
    ctx->video.enc_ctx->sample_aspect_ratio = (AVRational){1, 1};
    ctx->video.enc_ctx->gop_size = FFMAX(12, (int)av_q2d(frame_rate) * 2);
    ctx->video.enc_ctx->max_b_frames = 2;
    ctx->video.enc_ctx->thread_count = FFMPEG_WASM_PTHREADS ? FFMPEG_WASM_ENCODER_THREAD_COUNT : 1;
    ctx->video.enc_ctx->thread_type = FFMPEG_WASM_PTHREADS ? FF_THREAD_FRAME : 0;
    if (ctx->options->video_bitrate_kbps > 0)
        ctx->video.enc_ctx->bit_rate = (int64_t)ctx->options->video_bitrate_kbps * 1000;
    else {
        snprintf(crf_text, sizeof(crf_text), "%d", ctx->options->crf);
        av_dict_set(&encoder_options, "crf", crf_text, 0);
    }
    av_dict_set(&encoder_options, "preset", x264_preset_for_speed(ctx->options), 0);
    if (FFMPEG_WASM_PTHREADS) {
        char x264_params[64];
        snprintf(x264_params, sizeof(x264_params), "lookahead-threads=%d",
                 FFMPEG_WASM_X264_LOOKAHEAD_THREAD_COUNT);
        av_dict_set(&encoder_options, "x264-params", x264_params, 0);
    }
    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->video.enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_log(NULL, AV_LOG_INFO,
           "opening multi-input libx264 encoder: threads=%d thread_type=%s lookahead_threads=%d size=%dx%d time_base=%d/%d\n",
           ctx->video.enc_ctx->thread_count,
           FFMPEG_WASM_PTHREADS ? "frame" : "single",
           FFMPEG_WASM_PTHREADS ? FFMPEG_WASM_X264_LOOKAHEAD_THREAD_COUNT : 0,
           ctx->video.enc_ctx->width, ctx->video.enc_ctx->height,
           ctx->video.enc_ctx->time_base.num, ctx->video.enc_ctx->time_base.den);
    ret = avcodec_open2(ctx->video.enc_ctx, encoder, &encoder_options);
    av_dict_free(&encoder_options);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR,
               "Could not open multi-input libx264 encoder: %s (%d)\n",
               av_err2str(ret), ret);
        return ret;
    }
    ret = avcodec_parameters_from_context(ctx->video.stream->codecpar, ctx->video.enc_ctx);
    if (ret < 0)
        return ret;
    ctx->video.stream->time_base = ctx->video.enc_ctx->time_base;
    ctx->video.frame = av_frame_alloc();
    ctx->video.packet = av_packet_alloc();
    return (!ctx->video.frame || !ctx->video.packet) ? AVERROR(ENOMEM) : 0;
}

static int multi_setup_audio_encoder(MultiContext *ctx)
{
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVChannelLayout sink_layout;
    int ret;

    if (!ctx->have_audio)
        return 0;
    if (!encoder)
        return AVERROR_ENCODER_NOT_FOUND;
    ctx->audio.stream = avformat_new_stream(ctx->ofmt_ctx, NULL);
    if (!ctx->audio.stream)
        return AVERROR(ENOMEM);
    ctx->audio.enc_ctx = avcodec_alloc_context3(encoder);
    if (!ctx->audio.enc_ctx)
        return AVERROR(ENOMEM);

    ctx->audio.enc_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    ctx->audio.enc_ctx->sample_rate = av_buffersink_get_sample_rate(ctx->audio.sink_ctx);
    if (ctx->audio.enc_ctx->sample_rate <= 0)
        ctx->audio.enc_ctx->sample_rate = 48000;
    ctx->audio.enc_ctx->sample_fmt = (enum AVSampleFormat)av_buffersink_get_format(ctx->audio.sink_ctx);
    if (ctx->audio.enc_ctx->sample_fmt == AV_SAMPLE_FMT_NONE)
        ctx->audio.enc_ctx->sample_fmt = choose_sample_format(encoder);
    ctx->audio.enc_ctx->time_base = (AVRational){1, ctx->audio.enc_ctx->sample_rate};
    ctx->audio.enc_ctx->bit_rate = (int64_t)ctx->options->audio_bitrate_kbps * 1000;
    ctx->audio.enc_ctx->thread_count = 1;
    ctx->audio.enc_ctx->thread_type = 0;
    memset(&sink_layout, 0, sizeof(sink_layout));
    ret = av_buffersink_get_ch_layout(ctx->audio.sink_ctx, &sink_layout);
    if (ret < 0 || sink_layout.nb_channels <= 0) {
        av_channel_layout_uninit(&sink_layout);
        av_channel_layout_default(&sink_layout, 2);
    }
    ret = av_channel_layout_copy(&ctx->audio.enc_ctx->ch_layout, &sink_layout);
    av_channel_layout_uninit(&sink_layout);
    if (ret < 0)
        return ret;
    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->audio.enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    ret = avcodec_open2(ctx->audio.enc_ctx, encoder, NULL);
    if (ret < 0)
        return ret;
    ret = avcodec_parameters_from_context(ctx->audio.stream->codecpar, ctx->audio.enc_ctx);
    if (ret < 0)
        return ret;
    ctx->audio.stream->time_base = ctx->audio.enc_ctx->time_base;
    if (ctx->audio.enc_ctx->frame_size > 0)
        av_buffersink_set_frame_size(ctx->audio.sink_ctx, ctx->audio.enc_ctx->frame_size);
    ctx->audio.frame = av_frame_alloc();
    ctx->audio.packet = av_packet_alloc();
    return (!ctx->audio.frame || !ctx->audio.packet) ? AVERROR(ENOMEM) : 0;
}

static int multi_open_output(MultiContext *ctx)
{
    AVDictionary *mux_options = NULL;
    int ret;

    avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, "mp4", ctx->options->output_path);
    if (!ctx->ofmt_ctx)
        return AVERROR_UNKNOWN;
    ret = multi_setup_video_encoder(ctx);
    if (ret < 0)
        return ret;
    ret = multi_setup_audio_encoder(ctx);
    if (ret < 0)
        return ret;
    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, ctx->options->output_path, AVIO_FLAG_WRITE);
        if (ret < 0)
            return ret;
    }
    if (ctx->options->faststart)
        av_dict_set(&mux_options, "movflags", "+faststart", 0);
    ret = avformat_write_header(ctx->ofmt_ctx, &mux_options);
    av_dict_free(&mux_options);
    return ret;
}

static int multi_encode_output(MultiContext *ctx, MultiOutput *output,
                               AVFrame *frame, int flush)
{
    int ret;
    if (!output->enc_ctx)
        return 0;
    if (frame && frame->pts != AV_NOPTS_VALUE) {
        AVRational sink_tb = av_buffersink_get_time_base(output->sink_ctx);
        frame->pts = av_rescale_q(frame->pts, sink_tb, output->enc_ctx->time_base);
    }
    ret = avcodec_send_frame(output->enc_ctx, flush ? NULL : frame);
    if (ret < 0 && ret != AVERROR_EOF)
        return ret;
    while (ret >= 0) {
        av_packet_unref(output->packet);
        ret = avcodec_receive_packet(output->enc_ctx, output->packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        output->packet->stream_index = output->stream->index;
        av_packet_rescale_ts(output->packet, output->enc_ctx->time_base, output->stream->time_base);
        ret = av_interleaved_write_frame(ctx->ofmt_ctx, output->packet);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int multi_drain_sinks(MultiContext *ctx)
{
    int ret;
    if (ctx->have_video) {
        while ((ret = av_buffersink_get_frame(ctx->video.sink_ctx, ctx->video.frame)) >= 0) {
            ret = multi_encode_output(ctx, &ctx->video, ctx->video.frame, 0);
            av_frame_unref(ctx->video.frame);
            if (ret < 0)
                return ret;
        }
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return ret;
    }
    if (ctx->have_audio) {
        while ((ret = av_buffersink_get_frame(ctx->audio.sink_ctx, ctx->audio.frame)) >= 0) {
            ret = multi_encode_output(ctx, &ctx->audio, ctx->audio.frame, 0);
            av_frame_unref(ctx->audio.frame);
            if (ret < 0)
                return ret;
        }
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            return ret;
    }
    return 0;
}

static int multi_frame_in_range(MultiContext *ctx, MultiInput *input,
                                MultiStream *stream, AVFrame *frame)
{
    AVStream *avstream = input->fmt_ctx->streams[stream->stream_index];
    int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
        ? frame->best_effort_timestamp : frame->pts;
    int64_t origin = avstream->start_time != AV_NOPTS_VALUE ? avstream->start_time : 0;
    int64_t start = av_rescale_q((int64_t)llround(ctx->options->start_time_seconds * AV_TIME_BASE),
                                 AV_TIME_BASE_Q, avstream->time_base);
    int64_t duration = ctx->options->duration_seconds > 0.0
        ? av_rescale_q((int64_t)llround(ctx->options->duration_seconds * AV_TIME_BASE),
                       AV_TIME_BASE_Q, avstream->time_base)
        : 0;
    int64_t relative;

    if (pts == AV_NOPTS_VALUE)
        return 1;
    if (pts < origin + start)
        return 0;
    if (duration > 0 && pts > origin + start + duration)
        return -1;
    relative = pts - origin - start;
    frame->pts = av_rescale_q(relative, avstream->time_base, stream->filter_time_base);
    frame->time_base = stream->filter_time_base;
    return 1;
}

static int multi_feed_decoded(MultiContext *ctx, MultiInput *input,
                              MultiStream *stream, AVPacket *packet)
{
    int ret;
    int range;
    ret = avcodec_send_packet(stream->dec_ctx, packet);
    if (ret < 0 && ret != AVERROR_EOF)
        return ret;
    while (ret >= 0) {
        ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return ret;
        range = multi_frame_in_range(ctx, input, stream, stream->dec_frame);
        if (range > 0) {
            ret = av_buffersrc_add_frame_flags(stream->src_ctx, stream->dec_frame,
                                               AV_BUFFERSRC_FLAG_KEEP_REF);
            av_frame_unref(stream->dec_frame);
            if (ret < 0)
                return ret;
            ret = multi_drain_sinks(ctx);
            if (ret < 0)
                return ret;
        } else {
            av_frame_unref(stream->dec_frame);
        }
    }
    return 0;
}

static void multi_report_progress(MultiContext *ctx, int input_index, AVPacket *packet)
{
    MultiInput *input;
    AVStream *stream;
    int64_t ts;
    int64_t us;
    int64_t start_us;
    double progress;

    if (input_index != ctx->main_input_index || ctx->progress_duration_us <= 0 || !packet)
        return;
    input = &ctx->inputs[input_index];
    if (packet->stream_index < 0 || packet->stream_index >= (int)input->fmt_ctx->nb_streams)
        return;
    stream = input->fmt_ctx->streams[packet->stream_index];
    ts = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (ts == AV_NOPTS_VALUE)
        return;
    us = av_rescale_q(ts, stream->time_base, AV_TIME_BASE_Q);
    start_us = (int64_t)llround(ctx->options->start_time_seconds * AV_TIME_BASE);
    progress = (us - start_us) / (double)ctx->progress_duration_us;
    progress = FFMAX(0.0, FFMIN(0.999, progress));
    if (progress - ctx->last_progress >= 0.01 || progress == 0.0) {
        printf(PROGRESS_PREFIX " %.6f\n", progress);
        fflush(stdout);
        ctx->last_progress = progress;
    }
}

static int multi_flush_stream(MultiContext *ctx, MultiInput *input, MultiStream *stream)
{
    int ret;
    if (!stream->present || stream->source_eof)
        return 0;
    ret = multi_feed_decoded(ctx, input, stream, NULL);
    if (ret < 0)
        return ret;
    ret = av_buffersrc_add_frame_flags(stream->src_ctx, NULL, 0);
    if (ret < 0 && ret != AVERROR_EOF)
        return ret;
    stream->source_eof = 1;
    return multi_drain_sinks(ctx);
}

static int multi_transcode(MultiContext *ctx)
{
    int active = ctx->input_count;
    int ret = 0;
    int i;

    printf(PROGRESS_PREFIX " 0.000000\n");
    fflush(stdout);
    while (active > 0) {
        int made_progress = 0;
        for (i = 0; i < ctx->input_count; i++) {
            MultiInput *input = &ctx->inputs[i];
            if (input->demux_eof)
                continue;
            av_packet_unref(input->packet);
            ret = av_read_frame(input->fmt_ctx, input->packet);
            if (ret == AVERROR_EOF) {
                input->demux_eof = 1;
                active--;
                ret = multi_flush_stream(ctx, input, &input->video);
                if (ret < 0) return ret;
                ret = multi_flush_stream(ctx, input, &input->audio);
                if (ret < 0) return ret;
                continue;
            }
            if (ret < 0)
                return ret;
            made_progress = 1;
            multi_report_progress(ctx, i, input->packet);
            if (input->video.present && input->packet->stream_index == input->video.stream_index)
                ret = multi_feed_decoded(ctx, input, &input->video, input->packet);
            else if (input->audio.present && input->packet->stream_index == input->audio.stream_index)
                ret = multi_feed_decoded(ctx, input, &input->audio, input->packet);
            else
                ret = 0;
            if (ret < 0)
                return ret;
        }
        if (!made_progress && active > 0)
            break;
    }

    ret = multi_drain_sinks(ctx);
    if (ret < 0)
        return ret;
    if (ctx->have_video) {
        ret = multi_encode_output(ctx, &ctx->video, NULL, 1);
        if (ret < 0)
            return ret;
    }
    if (ctx->have_audio) {
        ret = multi_encode_output(ctx, &ctx->audio, NULL, 1);
        if (ret < 0)
            return ret;
    }
    ret = av_write_trailer(ctx->ofmt_ctx);
    if (ret >= 0) {
        printf(PROGRESS_PREFIX " 1.000000\n");
        fflush(stdout);
    }
    return ret;
}

static void multi_free_stream(MultiStream *stream)
{
    avcodec_free_context(&stream->dec_ctx);
    av_frame_free(&stream->dec_frame);
    stream->src_ctx = NULL;
}

static void multi_free_output(MultiOutput *output)
{
    avcodec_free_context(&output->enc_ctx);
    av_frame_free(&output->frame);
    av_packet_free(&output->packet);
    output->sink_ctx = NULL;
    output->stream = NULL;
}

static void multi_cleanup(MultiContext *ctx)
{
    int i;
    for (i = 0; i < ctx->input_count; i++) {
        multi_free_stream(&ctx->inputs[i].video);
        multi_free_stream(&ctx->inputs[i].audio);
        av_packet_free(&ctx->inputs[i].packet);
        avformat_close_input(&ctx->inputs[i].fmt_ctx);
    }
    multi_free_output(&ctx->video);
    multi_free_output(&ctx->audio);
    avfilter_graph_free(&ctx->graph);
    if (ctx->ofmt_ctx && !(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ctx->ofmt_ctx->pb);
    avformat_free_context(ctx->ofmt_ctx);
    ctx->ofmt_ctx = NULL;
}

static int run_multi_input(RunnerOptions *options)
{
    MultiContext ctx;
    int ret;

    memset(&ctx, 0, sizeof(ctx));
    ctx.options = options;
    ctx.input_count = options->input_count;
    ctx.main_input_index = options->main_input_index;
    multi_reset_output(&ctx.video, AVMEDIA_TYPE_VIDEO);
    multi_reset_output(&ctx.audio, AVMEDIA_TYPE_AUDIO);

    if (ctx.input_count < 2 || ctx.input_count > MULTI_MAX_INPUTS ||
        !options->filter_complex || !*options->filter_complex ||
        !options->video_map || !*options->video_map) {
        av_log(NULL, AV_LOG_ERROR, "Invalid multi-input Filter Builder request\n");
        return AVERROR(EINVAL);
    }

    av_log(NULL, AV_LOG_INFO, "multi-input graph: inputs=%d main=%d\n",
           ctx.input_count, ctx.main_input_index);
    ret = multi_open_inputs(&ctx);
    if (ret < 0)
        goto end;
    ret = multi_build_graph(&ctx);
    if (ret < 0)
        goto end;
    ret = multi_open_output(&ctx);
    if (ret < 0)
        goto end;
    ret = multi_transcode(&ctx);

end:
    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Multi-input Filter Builder failed: %s\n", av_err2str(ret));
    multi_cleanup(&ctx);
    return ret;
}
