#include "../../include/codec.h"

Codec *init_codec(LogLevel level)
{
    Codec *codec = (Codec *)malloc(sizeof(Codec));

    codec->in_codec = NULL;
    codec->out_codec = NULL;
    codec->in_codec_ctx = NULL;
    codec->out_codec_ctx = NULL;
    codec->out_stream = NULL;

    // 初始化FFmpeg
    if (avformat_network_init() < 0)
    {
        LOG(logger, LOG_ERROR, "Codec init failed");
        free(codec);
        return NULL;
    }

    switch (level)
    {
    case LOG_DEBUG:
        av_log_set_level(AV_LOG_DEBUG);
        break;
    case LOG_INFO:
        av_log_set_level(AV_LOG_INFO);
        break;
    case LOG_WARNING:
        av_log_set_level(AV_LOG_WARNING);
        break;
    case LOG_ERROR:
        av_log_set_level(AV_LOG_ERROR);
        break;
    default:
        av_log_set_level(AV_LOG_INFO);
        break;
    }

    LOG(logger, LOG_INFO, "Codec init successfully");

    return codec;
}

void destroy_codec(Codec *codec)
{
    if (avformat_network_deinit() < 0)
        LOG(logger, LOG_WARNING, "Deinit ffmpeg failed");
    free(codec);
}

int open_codec(Codec *codec, Config config)
{
    codec->save_time = config.save_time;

    // 配置编码器
    codec->out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec->out_codec)
    {
        LOG(logger, LOG_ERROR, "Find `H264` encoder failed");
        return -1;
    }

    // 配置编码器上下文
    codec->out_codec_ctx = avcodec_alloc_context3(codec->out_codec);
    if (!codec->out_codec_ctx)
    {
        LOG(logger, LOG_ERROR, "Alloc encoder context failed");
        return -1;
    }
    codec->out_codec_ctx->width = config.width;
    codec->out_codec_ctx->height = config.height;
    codec->out_codec_ctx->time_base = config.time_base;
    codec->out_codec_ctx->framerate = av_inv_q(config.time_base);
    codec->out_codec_ctx->pix_fmt = AV_PIX_FMT_YUV422P;

    if (av_opt_set(codec->out_codec_ctx->priv_data, "tune", "zerolatency", 0) < 0)
    {
        LOG(logger, LOG_ERROR, "Prev config for codec failed");
        return -1;
    }

    // 配置解码器
    enum AVCodecID id = (config.pix_format == MJPEG) ? AV_CODEC_ID_MJPEG : AV_CODEC_ID_YUV4;
    codec->in_codec = avcodec_find_decoder(id);
    if (!codec->out_codec)
    {
        LOG(logger, LOG_ERROR, "Find `%s` decoder failed", (config.pix_format == MJPEG) ? "MJPEG" : "YUYV");
        return -1;
    }

    // 配置解码器上下文
    codec->in_codec_ctx = avcodec_alloc_context3(codec->in_codec);
    if (!codec->out_codec_ctx)
    {
        LOG(logger, LOG_ERROR, "Alloc decoder context failed");
        return -1;
    }
    codec->in_codec_ctx->width = config.width;
    codec->in_codec_ctx->height = config.height;
    codec->in_codec_ctx->time_base = config.time_base;
    codec->in_codec_ctx->framerate = av_inv_q(config.time_base);
    codec->in_codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ422P;

    // 打开编码器
    if (avcodec_open2(codec->out_codec_ctx, codec->out_codec, NULL) < 0)
    {
        LOG(logger, LOG_ERROR, "Open encoder failed");
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(codec->in_codec_ctx, codec->in_codec, NULL) < 0)
    {
        LOG(logger, LOG_ERROR, "Open decoder failed");
        return -1;
    }

    // 创建用于存储解码后图像的AVFrame
    codec->decoded_frame = av_frame_alloc();

    return 0;
}

int dispose_codec(Codec *codec, BufType frame, int64_t time_stamp, AVFormatContext *frm_ctx_set, int length)
{
    // 设置解码输入
    AVPacket *packet = av_packet_alloc();
    packet->data = (uint8_t *)frame.start;
    packet->size = frame.length;

    // 解码MJPEG图像
    int ret = avcodec_send_packet(codec->in_codec_ctx, packet);
    if (ret < 0)
    {
        LOG(logger, LOG_ERROR, "Sending a packet for decoding failed");
        return -1;
    }

    ret = avcodec_receive_frame(codec->in_codec_ctx, codec->decoded_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        // 需要更多输入数据或解码完成
        av_packet_unref(packet);
        destroy_buf(&frame);
        return -2;
    }
    else if (ret < 0)
    {
        LOG(logger, LOG_ERROR, "Error during decoding");
        return -1;
    }

    // 编码H.264图像
    ret = avcodec_send_frame(codec->out_codec_ctx, codec->decoded_frame);
    if (ret < 0)
    {
        LOG(logger, LOG_ERROR, "Error sending a frame for encoding");
        return -1;
    }

    AVPacket *encoded_packet = av_packet_alloc();
    ret = avcodec_receive_packet(codec->out_codec_ctx, encoded_packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        // 需要更多输入数据或编码完成
        av_packet_unref(encoded_packet);
        destroy_buf(&frame);
        return -2;
    }
    else if (ret == 0)
    {
        // 设置时间戳
        encoded_packet->pts = av_rescale_q(time_stamp, codec->out_codec_ctx->time_base, codec->out_stream->time_base);
        encoded_packet->dts = encoded_packet->pts;

        // 写入编码后的帧到输出流
        for (int i = 0; i < length; i++)
        {
            ret = av_interleaved_write_frame(frm_ctx_set, encoded_packet);
            if (ret < 0)
            {
                LOG(logger, LOG_ERROR, "Error writing encoded frame");
                return -1;
            }
        }
    }
    else if (ret < 0)
    {
        LOG(logger, LOG_ERROR, "Error during encoding");
        return -1;
    }

    av_packet_unref(packet);
    av_packet_unref(encoded_packet);
    av_packet_free(&packet);
    av_packet_free(&encoded_packet);

    return 0;
}

void close_codec(Codec *codec)
{
    av_frame_free(&(codec->decoded_frame));
    avcodec_free_context(&(codec->in_codec_ctx));
    avcodec_free_context(&(codec->out_codec_ctx));
}

int open_output(Codec *codec, Config config, AVFormatContext **frm_ctx, const char *path, const char *format)
{
    if (avformat_alloc_output_context2(frm_ctx, NULL, format, path) < 0)
    {
        LOG(logger, LOG_ERROR, "Open `%s` output context failed", format);
        return -1;
    }
    AVOutputFormat *out_fmt = av_guess_format(format, NULL, NULL);
    if (!out_fmt)
    {
        LOG(logger, LOG_ERROR, "Find format `%s` failed", format);
        return -1;
    }
    (*frm_ctx)->oformat = out_fmt;
    if (avio_open(&(*frm_ctx)->pb, path, AVIO_FLAG_WRITE) < 0)
    {
        LOG(logger, LOG_ERROR, "Open output `%s` failed", path);
        return -1;
    }

    codec->out_stream = avformat_new_stream(*frm_ctx, NULL);
    if (!codec->out_stream)
    {
        LOG(logger, LOG_ERROR, "Add new out stream failed");
        return -1;
    }
    // 设置视频参数，例如分辨率、帧率、码率等
    codec->out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codec->out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    codec->out_stream->codecpar->width = config.width;
    codec->out_stream->codecpar->height = config.height;
    codec->out_stream->codecpar->format = AV_PIX_FMT_YUV422P;
    codec->out_stream->codecpar->bit_rate = 500000;
    codec->out_stream->time_base = config.time_base;
    if (avformat_write_header(*frm_ctx, NULL) < 0)
    {
        LOG(logger, LOG_ERROR, "Write head failed");
        return -1;
    }
    return 0;
}

int close_output(AVFormatContext *frm_ctx)
{
    if (av_write_trailer(frm_ctx) < 0)
    {
        LOG(logger, LOG_ERROR, "Write trailer failed");
        return -1;
    }
    if (avio_close(frm_ctx->pb) < 0)
    {
        LOG(logger, LOG_ERROR, "Close io failed");
        return -1;
    }
    avformat_free_context(frm_ctx);
    return 0;
}