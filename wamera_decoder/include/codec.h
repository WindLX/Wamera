#ifndef CODEC_H
#define CODEC_H

#include <stdlib.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

#include "./tool.h"
#include "./logger.h"

typedef struct Codec
{
    char *rtmp_path;
    char *dir_path;
    unsigned int save_time;
    AVCodec *in_codec;
    AVCodec *out_codec;
    AVCodecContext *in_codec_ctx;
    AVCodecContext *out_codec_ctx;
    AVStream *out_stream;
    AVFrame *decoded_frame;
} Codec;

/**
 * @brief init_codec 初始化编解码器
 * @param level ffmpeg的日志等级
 * @return Codec* 初始化完成的编解码器
 */
Codec *init_codec(LogLevel level);

/**
 * @brief destroy_codec 释放编解码器内存
 * @param codec 待释放的编解码器
 */
void destroy_codec(Codec *codec);

/**
 * @brief open_codec 打开编解码器
 * @param codec 待打开的编解码器
 * @param config 设置
 * @return int 启动成功返回0, 失败返回1
 */
int open_codec(Codec *codec, Config config);

/**
 * @brief dispose_codec 处理编解码
 * @param codec 工作的编解码器
 * @param frame 处理帧
 * @param time_stamp 处理帧的时间戳
 * @param frm_ctx_set 写入数据的地址
 * @return int 处理成功返回0, 失败返回-1, 跳过返回-2
 */
int dispose_codec(Codec *codec, BufType frame, int64_t time_stamp, AVFormatContext *frm_ctx_set, int length);

/**
 * @brief close_codec 关闭编解码器
 * @param codec 待关闭的编解码器
 */
void close_codec(Codec *codec);

/**
 * @brief open_output 配置输出上下文
 * @param frm_ctx 输出上下文格式
 * @param path 输出地址
 * @param format 输出格式
 * @return int 成功返回0, 失败返回-1
 */
int open_output(Codec *codec, Config config, AVFormatContext **frm_ctx, const char *path, const char *format);

/**
 * @brief close_output 关闭输出上下文
 * @param frm_ctx 输出下文格式
 * @return int 成功返回0，失败返回-1
 */
int close_output(AVFormatContext *frm_ctx);

#endif