/*
 * Experimental, video-only, memory-bounded reverse / forward chunk runner.
 * Reuses the public-libav Filter Builder core (no upstream ffmpeg CLI).
 *
 * This adapter is deliberately narrow: one input, one short reversed chunk,
 * H.264 MP4, and no audio. --direction forward emits a short forward
 * chunk for ping-pong composition; default reverse behavior is unchanged.
 * The caller orders and joins chunks after frame/timestamp checks.
 *
 * The embedded core remains the FFmpeg/x264 GPL-2.0-or-later binary.
 * NO mobile/WASM feasibility has been established by the existence of this file.
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the proven demux/decode/filter/encode implementation shared. */
#define main filter_builder_main
#include "ffmpeg-filter-builder.c"
#undef main

#include "video-reverse-join.inc"

#define REVERSE_MAX_DURATION 2.0
#define REVERSE_MAX_BUFFER_MIB 96.0

static void reverse_usage(void)
{
    fprintf(stderr,
            "Experimental video-reverse single-chunk runner\n"
            "Usage: --input FILE --output MP4 --start-time SECONDS --duration SECONDS\n"
            "       --width EVEN --height EVEN --fps INTEGER [--frames COUNT] [--crf 0..51]\n"
            "       [--direction reverse|forward] (default reverse)\n"
            "       --audio-segment --input FILE --output MP4 --start-time S\n"
            "         --duration 0.25..10 --direction reverse|forward|pingpong\n"
            "         pingpong additionally requires --fps N --frames N\n"
            "         Requires actual AAC-compatible source audio; produces\n"
            "         experimental AAC and disposable tiny H.264 temp video.\n"
            "       --inspect-audio --input FILE --output META.json\n"
            "       --mux-audio --video JOINED.mp4 --audio AUDIO_CANDIDATE.mp4\n"
            "         --output AV.mp4 --expected-frames N --expected-duration S\n"
            "  duration: 0.1..2.0 seconds; one chunk only; output H.264 MP4, no audio.\n"
            "  --frames forces exact output count (2..120, matching fps*duration).\n"
            "  estimate of cached YUV420 frames must not exceed 96 MiB.\n"
            "  input source codec/rotation and actual peak RAM are not guaranteed.\n");
}

static int parse_finite(const char *text, double low, double high, double *out)
{
    char *end = NULL;
    double value;
    if (!text || !*text) return -1;
    errno = 0;
    value = strtod(text, &end);
    if (errno || !end || *end || !isfinite(value) || value < low || value > high)
        return -1;
    *out = value;
    return 0;
}


/* Count encoded H.264 packets instead of trusting container duration.
 * Abort and delete chunks that are a frame short/long. */
/*
 * Per-segment reversal remains capped at 120 frames. A joined video can
 * legitimately contain up to 960, and ping-pong uses 2*N-2 frames; sharing
 * the old 120-frame limit incorrectly rejected valid long AAC MP4 exports.
 * Pass the intended bound explicitly instead of weakening chunk safety.
 */
static int verify_reverse_frame_count(const char *file, int expected, int max_frames)
{
    AVFormatContext *ctx = NULL;
    AVPacket *pkt = NULL;
    int index = -1, actual = 0, ret;
    if (!file || expected <= 0 || max_frames < expected || max_frames > 960)
        return AVERROR(EINVAL);
    ret = avformat_open_input(&ctx, file, NULL, NULL);
    if (ret < 0) goto end;
    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) goto end;
    index = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (index < 0) { ret = index; goto end; }
    pkt = av_packet_alloc();
    if (!pkt) { ret = AVERROR(ENOMEM); goto end; }
    while ((ret = av_read_frame(ctx, pkt)) >= 0) {
        if (pkt->stream_index == index) actual++;
        av_packet_unref(pkt);
        if (actual > max_frames) {
            av_log(NULL, AV_LOG_ERROR,
                   "MP4 video frame count exceeded expected bound %d.\n", max_frames);
            ret = AVERROR_INVALIDDATA;
            break;
        }
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret >= 0 && actual != expected) {
        av_log(NULL, AV_LOG_ERROR, "Reverse frame count mismatch: expected=%d actual=%d.\n",
               expected, actual);
        ret = AVERROR_INVALIDDATA;
    }
end:
    av_packet_free(&pkt);
    avformat_close_input(&ctx);
    return ret;
}

/*
 * v0.5 audio-only candidate extraction:
 * Filter Builder's proven AAC path can encode a short selected audio span.
 * Until a separate libav-only audio pipeline is ready, this also encodes a
 * disposable 16x16/1fps video stream in the temporary MP4. The application
 * must ONLY use the AAC stream when joining it with its real reverse result.
 * Do not claim this is memory-optimal or expose the discarded placeholder.
 *
 * The filter and all options are selected internally: there is no general
 * ffmpeg command execution and no arbitrary filter syntax accepted.
 */
static int verify_audio_segment(const char *path)
{
    AVFormatContext *ctx = NULL;
    AVPacket *packet = NULL;
    int idx, count = 0, ret;
    ret = avformat_open_input(&ctx, path, NULL, NULL);
    if (ret < 0) goto done;
    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) goto done;
    idx = av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (idx < 0 || ctx->streams[idx]->codecpar->codec_id != AV_CODEC_ID_AAC) {
        av_log(NULL, AV_LOG_ERROR,
               "Selected source has no decodable audio, or output lacks AAC.\n");
        ret = AVERROR_STREAM_NOT_FOUND;
        goto done;
    }
    if (ctx->streams[idx]->codecpar->ch_layout.nb_channels < 1 ||
        ctx->streams[idx]->codecpar->ch_layout.nb_channels > 2) {
        ret = AVERROR_INVALIDDATA;
        goto done;
    }
    packet = av_packet_alloc();
    if (!packet) { ret = AVERROR(ENOMEM); goto done; }
    while ((ret = av_read_frame(ctx, packet)) >= 0) {
        if (packet->stream_index == idx) count++;
        av_packet_unref(packet);
    }
    if (ret == AVERROR_EOF) ret = 0;
    if (ret >= 0 && count < 1) ret = AVERROR_INVALIDDATA;
done:
    av_packet_free(&packet);
    avformat_close_input(&ctx);
    return ret;
}

static int reverse_audio_segment(int argc, char **argv)
{
    const char *input = NULL, *output = NULL, *start_txt = NULL;
    const char *duration_txt = NULL, *direction = "reverse";
    double start = 0.0, duration = 0.0, frames = 0.0, fps = 0.0;
    char filter[800];
    int i;
    for (i = 2; i < argc; ++i) {
        const char *flag = argv[i], *value;
        if (i + 1 >= argc) return 2;
        value = argv[++i];
        if (!strcmp(flag, "--input")) input = value;
        else if (!strcmp(flag, "--output")) output = value;
        else if (!strcmp(flag, "--start-time")) start_txt = value;
        else if (!strcmp(flag, "--duration")) duration_txt = value;
        else if (!strcmp(flag, "--direction")) {
            if (strcmp(value, "reverse") && strcmp(value, "forward") &&
                strcmp(value, "pingpong")) return 2;
            direction = value;
        } else if (!strcmp(flag, "--fps")) {
            if (parse_finite(value, 1, 60, &fps)<0 || floor(fps)!=fps)return 2;
        } else if (!strcmp(flag, "--frames")) {
            if (parse_finite(value, 4, 600, &frames)<0 || floor(frames)!=frames)return 2;
        } else {
            fprintf(stderr, "Unsupported audio-segment option: %s\n", flag);
            return 2;
        }
    }
    if (!input || !*input || !output || !*output || !start_txt ||
        !duration_txt || !strcmp(input, output) ||
        parse_finite(start_txt, 0, 2592000, &start) < 0 ||
        parse_finite(duration_txt, 0.25, 10.0, &duration) < 0)
        return 2;
    /* Buffer at most 10s of decoded audio. Two 8ms ramps guard joins. */
    if (!strcmp(direction,"pingpong")) {
        if(fps<1||frames<4||fabs(duration*fps-frames)>.05)return 2;
        const double skipped=1.0/fps, return_duration=(frames-2.0)/fps;
        snprintf(filter,sizeof(filter),
                 "asplit=2[forward][return_input];"
                 "[return_input]areverse,asetpts=PTS-STARTPTS,"
                 "atrim=start=%.6f:duration=%.6f,asetpts=PTS-STARTPTS,"
                 "afade=t=in:st=0:d=0.008[back];"
                 "[forward]afade=t=out:st=%.6f:d=0.008[fwd];"
                 "[fwd][back]concat=n=2:v=0:a=1,asetpts=PTS-STARTPTS",
                 skipped,return_duration,duration-.008);
    } else {
        if(fps>0||frames>0)return 2;
        snprintf(filter, sizeof(filter),
                 "%sasetpts=PTS-STARTPTS,afade=t=in:st=0:d=0.008,"
                 "afade=t=out:st=%.6f:d=0.008",
                 !strcmp(direction, "reverse") ? "areverse," : "",
                 duration - .008);
    }
    char *delegated[] = {
        argv[0], "--input", (char *)input, "--output", (char *)output,
        "--start-time", (char *)start_txt, "--duration", (char *)duration_txt,
        "--video-filter", "scale=16:16:flags=bicubic",
        "--fps", "1", "--crf", "43", "--audio-bitrate", "96",
        "--audio-filter", filter
    };
    int ret = filter_builder_main((int)(sizeof(delegated)/sizeof(delegated[0])), delegated);
    if (ret == 0)
        ret = verify_audio_segment(output) < 0 ? 1 : 0;
    if (ret != 0) remove(output);
    return ret;
}

/*
 * Inspect only the container headers and stream descriptors, not every media
 * packet. Unlike Filter Builder's detailed --inspect-output, this is safe
 * for very large Android WORKERFS sources with a selected short trim.
 */
static int inspect_source_audio(int argc, char **argv)
{
    const char *input = NULL, *output = NULL;
    AVFormatContext *ctx = avformat_alloc_context();
    FILE *result = NULL;
    int ret, aidx, vidx, i;
    if (!ctx) return 1;
    for (i=2;i<argc;++i) {
        const char *arg=argv[i], *value;
        if (++i>=argc) { ret=AVERROR(EINVAL); goto done; }
        value=argv[i];
        if(!strcmp(arg,"--input")) input=value;
        else if(!strcmp(arg,"--output")) output=value;
        else { ret=AVERROR(EINVAL); goto done; }
    }
    if(!input || !*input || !output || !*output || !strcmp(input,output)){
        ret=AVERROR(EINVAL);goto done;
    }
    ctx->probesize=1024*1024;
    ctx->max_analyze_duration=2*AV_TIME_BASE;
    ret=avformat_open_input(&ctx,input,NULL,NULL);
    if(ret<0)goto done;
    ret=avformat_find_stream_info(ctx,NULL);
    if(ret<0)goto done;
    vidx=av_find_best_stream(ctx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if(vidx<0){ret=vidx;goto done;}
    aidx=av_find_best_stream(ctx,AVMEDIA_TYPE_AUDIO,-1,vidx,NULL,0);
    result=fopen(output,"wb");
    if(!result){ret=AVERROR(errno);goto done;}
    fprintf(result,"{\"video\":\"%s\",\"audio\":",
            avcodec_get_name(ctx->streams[vidx]->codecpar->codec_id));
    if(aidx<0) fputs("null",result);
    else fprintf(result,"{\"codec\":\"%s\",\"channels\":%d}",
           avcodec_get_name(ctx->streams[aidx]->codecpar->codec_id),
           ctx->streams[aidx]->codecpar->ch_layout.nb_channels);
    fputs("}\n",result);
    ret=ferror(result)?AVERROR(EIO):0;
done:
    if(result)fclose(result);
    avformat_close_input(&ctx);
    if(ret<0 && output)remove(output);
    return ret<0?1:0;
}

/*
 * Safe, no-reencode MP4 video+AAC remux. The --audio source contains an
 * experimental tiny 16px temporary video which is deliberately ignored:
 * only its AAC stream is added to the *real* reverse/ping-pong H.264 result.
 * Requires exact caller video-frame count and bounded nominal duration.
 */
static int reverse_mux_audio(int argc, char **argv)
{
    const char *vfile=NULL,*afile=NULL,*output=NULL;
    int frame_limit=0,i,ret=0,vindex=-1,aindex=-1;
    int frames=0,sound_packets=0;
    double nominal_seconds=0;
    AVFormatContext *vfmt=NULL,*afmt=NULL,*ofmt=NULL;
    AVStream *srcv=NULL,*srca=NULL,*outv=NULL,*outa=NULL;
    AVPacket *packet=NULL;
    for(i=2;i<argc;++i) {
        const char *key=argv[i],*val;
        if(i+1>=argc)return 2;
        val=argv[++i];
        if(!strcmp(key,"--video")) vfile=val;
        else if(!strcmp(key,"--audio")) afile=val;
        else if(!strcmp(key,"--output")) output=val;
        else if(!strcmp(key,"--expected-frames")) {
            double number;
            if(parse_finite(val,2,960,&number)<0 || floor(number)!=number)return 2;
            frame_limit=(int)number;
        }else if(!strcmp(key,"--expected-duration")) {
            if(parse_finite(val,.25,20.0,&nominal_seconds)<0)return 2;
        }else return 2;
    }
    if(!vfile||!*vfile||!afile||!*afile||!output||!*output||
       !strcmp(vfile,afile)||!strcmp(vfile,output)||
       !strcmp(afile,output)||!frame_limit||!nominal_seconds)return 2;
    ret=avformat_open_input(&vfmt,vfile,NULL,NULL);
    if(ret<0)goto done;
    ret=avformat_find_stream_info(vfmt,NULL);
    if(ret<0)goto done;
    ret=avformat_open_input(&afmt,afile,NULL,NULL);
    if(ret<0)goto done;
    ret=avformat_find_stream_info(afmt,NULL);
    if(ret<0)goto done;
    vindex=av_find_best_stream(vfmt,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    aindex=av_find_best_stream(afmt,AVMEDIA_TYPE_AUDIO,-1,-1,NULL,0);
    if(vindex<0||aindex<0){ret=AVERROR_STREAM_NOT_FOUND;goto done;}
    srcv=vfmt->streams[vindex];
    srca=afmt->streams[aindex];
    if(srcv->codecpar->codec_id!=AV_CODEC_ID_H264||
       srca->codecpar->codec_id!=AV_CODEC_ID_AAC||
       srca->codecpar->ch_layout.nb_channels<1||
       srca->codecpar->ch_layout.nb_channels>2){
        ret=AVERROR_INVALIDDATA;goto done;
    }
    /* Untrusted imported MP4s must not silently deliver unrelated durations. */
    if(srca->duration>0){
        double actual=srca->duration*av_q2d(srca->time_base);
        if(!isfinite(actual)||fabs(actual-nominal_seconds)>.20){
            av_log(NULL,AV_LOG_ERROR,"AAC duration %.3f differs from video %.3f\n",
                   actual,nominal_seconds);
            ret=AVERROR_INVALIDDATA;goto done;
        }
    }
    ret=avformat_alloc_output_context2(&ofmt,NULL,"mp4",output);
    if(ret<0||!ofmt){ret=AVERROR(ENOMEM);goto done;}
    outv=avformat_new_stream(ofmt,NULL);
    outa=avformat_new_stream(ofmt,NULL);
    if(!outv||!outa){ret=AVERROR(ENOMEM);goto done;}
    ret=avcodec_parameters_copy(outv->codecpar,srcv->codecpar);
    if(ret<0)goto done;
    ret=avcodec_parameters_copy(outa->codecpar,srca->codecpar);
    if(ret<0)goto done;
    outv->codecpar->codec_tag=0;
    outa->codecpar->codec_tag=0;
    outv->time_base=srcv->time_base;
    outa->time_base=srca->time_base;
    /* With at most 20 seconds the entire packet queue stays bounded and
     * FFmpeg can interleave chronologically even if streams are read apart. */
    ofmt->max_interleave_delta=0;
    ret=avio_open(&ofmt->pb,output,AVIO_FLAG_WRITE);
    if(ret<0)goto done;
    ret=avformat_write_header(ofmt,NULL);
    if(ret<0)goto done;
    packet=av_packet_alloc();
    if(!packet){ret=AVERROR(ENOMEM);goto done;}
    for(int side=0;side<2;side++){
        AVFormatContext *src=side?afmt:vfmt;
        AVStream *srcstream=side?srca:srcv;
        AVStream *dststream=side?outa:outv;
        int selected=side?aindex:vindex;
        int64_t origin=(srcstream->start_time!=AV_NOPTS_VALUE)?
                       srcstream->start_time:0;
        int64_t prev=INT64_MIN;
        while((ret=av_read_frame(src,packet))>=0){
            if(packet->stream_index!=selected){
                av_packet_unref(packet);continue;
            }
            if(packet->pts==AV_NOPTS_VALUE||packet->dts==AV_NOPTS_VALUE){
                ret=AVERROR_INVALIDDATA;
                av_packet_unref(packet);goto done;
            }
            packet->pts-=origin;
            packet->dts-=origin;
            av_packet_rescale_ts(packet,srcstream->time_base,dststream->time_base);
            if(packet->dts<=prev){
                ret=AVERROR_INVALIDDATA;
                av_packet_unref(packet);goto done;
            }
            prev=packet->dts;
            if(packet->duration<=0){
                AVRational time=side?(AVRational){1024,srca->codecpar->sample_rate}:
                    av_inv_q(av_guess_frame_rate(vfmt,srcv,NULL));
                if(time.den<=0||time.num<=0){
                    ret=AVERROR_INVALIDDATA;
                    av_packet_unref(packet);goto done;
                }
                packet->duration=av_rescale_q(1,time,dststream->time_base);
                if(packet->duration<=0){
                    ret=AVERROR_INVALIDDATA;
                    av_packet_unref(packet);goto done;
                }
            }
            packet->pos=-1;
            packet->stream_index=dststream->index;
            ret=av_interleaved_write_frame(ofmt,packet);
            av_packet_unref(packet);
            if(ret<0)goto done;
            if(side) sound_packets++;
            else frames++;
            if(frames>960||sound_packets>1200){
                ret=AVERROR_INVALIDDATA;goto done;
            }
        }
        if(ret!=AVERROR_EOF)goto done;
    }
    if(frames!=frame_limit||sound_packets<1){
        av_log(NULL,AV_LOG_ERROR,"AV remux packet mismatch %d/%d frames, %d audio\n",
               frames,frame_limit,sound_packets);
        ret=AVERROR_INVALIDDATA;goto done;
    }
    ret=av_write_trailer(ofmt);
    if(ret>=0){
        if(verify_reverse_frame_count(output,frame_limit,960)<0 ||
           verify_audio_segment(output)<0)
            ret=AVERROR_INVALIDDATA;
    }
done:
    av_packet_free(&packet);
    avformat_close_input(&vfmt);
    avformat_close_input(&afmt);
    if(ofmt){
        if(ofmt->pb)avio_closep(&ofmt->pb);
        avformat_free_context(ofmt);
    }
    if(ret<0){if(output)remove(output);return 1;}
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--inspect-audio"))
        return inspect_source_audio(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--mux-audio"))
        return reverse_mux_audio(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--audio-segment"))
        return reverse_audio_segment(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--concat"))
        return reverse_join_chunks(argc, argv);

    const char *input = NULL, *output = NULL;
    const char *start_text = NULL, *duration_text = NULL;
    const char *direction = "reverse";
    double start = 0, duration = 0, width = 0, height = 0, fps = 0, crf = 30, frames = 0;
    char filter[256], fps_text[16], crf_text[16];
    int i;

    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        reverse_usage();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        printf("experimental reverse chunk adapter; built with FFmpeg %s\n", av_version_info());
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *flag = argv[i];
        const char *value;
        if (i + 1 >= argc) { reverse_usage(); return 2; }
        value = argv[++i];
        if (!strcmp(flag, "--input")) input = value;
        else if (!strcmp(flag, "--direction")) {
            if (strcmp(value, "reverse") && strcmp(value, "forward")) {
                fprintf(stderr, "Only reverse and forward chunk directions are supported.\n");
                return 2;
            }
            direction = value;
        }
        else if (!strcmp(flag, "--output")) output = value;
        else if (!strcmp(flag, "--start-time")) start_text = value;
        else if (!strcmp(flag, "--duration")) duration_text = value;
        else if (!strcmp(flag, "--width")) {
            if (parse_finite(value, 16, 2048, &width) < 0) return 2;
        } else if (!strcmp(flag, "--height")) {
            if (parse_finite(value, 16, 2048, &height) < 0) return 2;
        } else if (!strcmp(flag, "--fps")) {
            if (parse_finite(value, 1, 60, &fps) < 0) return 2;
        } else if (!strcmp(flag, "--crf")) {
            if (parse_finite(value, 0, 51, &crf) < 0) return 2;
        } else if (!strcmp(flag, "--frames")) {
            if (parse_finite(value, 2, 120, &frames) < 0) return 2;
        } else {
            fprintf(stderr, "Unsupported reverse option: %s\n", flag);
            return 2;
        }
    }

    if (!input || !*input || !output || !*output || !start_text || !duration_text ||
        parse_finite(start_text, 0, 2592000, &start) < 0 ||
        parse_finite(duration_text, 0.1, REVERSE_MAX_DURATION, &duration) < 0 ||
        width != (int)width || height != (int)height ||
        ((int)width % 2) || ((int)height % 2) ||
        fps != (int)fps || crf != (int)crf || frames != (int)frames ||
        (frames > 0 && fabs(frames - fps * duration) > 0.05)) {
        reverse_usage();
        return 2;
    }

    /*
     * This is only a frame-buffer lower bound. Additional source decoder,
     * filter, encoder, Emscripten heap and browser memory still apply.
     * A nonzero upper bound makes accidental 10-second reverse impossible.
     */
    double estimated_mib = width * height * 1.5 * fps * duration / (1024.0 * 1024.0);
    if (estimated_mib > REVERSE_MAX_BUFFER_MIB) {
        fprintf(stderr, "Reverse chunk rejected: estimated frame cache %.1f MiB exceeds %.0f MiB limit.\n",
                estimated_mib, REVERSE_MAX_BUFFER_MIB);
        return 2;
    }

    /*
     * Filter Builder prepends precise trim and display-matrix autorotation.
     * Decode -> trim -> autorotate -> scale -> fps -> reverse -> reset PTS ->
     * final fps (from --fps) -> format/encoder. Scaling + fps BEFORE reverse
     * bound stored frame size/count; the final fps fixes frame durations.
     */
    /*
     * Identical scale/fps/trim/PTS chain and x264 settings for both
     * directions. For ping-pong, the caller removes return-leg endpoint
     * frames by choosing a frame-exact source window (not packet deletion).
     */
    const char *direction_filter = !strcmp(direction, "reverse")
                                      ? ",reverse,setpts=PTS-STARTPTS"
                                      : ",setpts=PTS-STARTPTS";
    if (frames > 0) {
        snprintf(filter, sizeof(filter),
                 "scale=%d:%d:flags=bicubic,fps=fps=%d,trim=end_frame=%d%s",
                 (int)width, (int)height, (int)fps, (int)frames, direction_filter);
    } else {
        snprintf(filter, sizeof(filter),
                 "scale=%d:%d:flags=bicubic,fps=fps=%d%s",
                 (int)width, (int)height, (int)fps, direction_filter);
    }
    snprintf(fps_text, sizeof(fps_text), "%d", (int)fps);
    snprintf(crf_text, sizeof(crf_text), "%d", (int)crf);

    char *delegated[] = {
        argv[0], "--input", (char *)input, "--output", (char *)output,
        "--start-time", (char *)start_text, "--duration", (char *)duration_text,
        "--video-filter", filter, "--fps", fps_text, "--crf", crf_text, "--no-audio"
    };
    int ret = filter_builder_main((int)(sizeof(delegated) / sizeof(delegated[0])), delegated);
    if (ret == 0 && frames > 0)
        ret = verify_reverse_frame_count(output, (int)frames, 120) < 0 ? 1 : 0;
    if (ret != 0) remove(output); /* Never expose incomplete or miscounted segments. */
    return ret;
}
