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
    AVCodec *in_codec;
    AVCodec *out_codec;
    AVCodecContext *in_codec_ctx;
    AVCodecContext *out_codec_ctx;
    AVFrame *decoded_frame;
} Codec;

typedef struct Output
{
    AVFormatContext *frm_ctx;
    AVStream *stream;
} Output;

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
 * @param output 输出器的数组
 * @param length 输出器的长度
 * @param frame 处理帧
 * @param time_stamp 处理帧的时间戳
 * @return int 处理成功返回0, 失败返回-1, 跳过返回-2
 */
int dispose_codec(Codec *codec, Output **output, unsigned int length, BufType frame, int64_t time_stamp);

/**
 * @brief close_codec 关闭编解码器
 * @param codec 待关闭的编解码器
 */
void close_codec(Codec *codec);

/**
 * @brief open_output 配置输出上下文
 * @param config 配置
 * @param path 输出地址
 * @param format 输出格式
 * @return Output 输出器
 */
Output *open_output(Config config, const char *path, const char *format);

/**
 * @brief close_output 关闭输出上下文
 * @param output 待关闭的输出器
 * @return int 成功返回0，失败返回-1
 */
int close_output(Output *output);

#endif