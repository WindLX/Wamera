#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/**
 * @brief LOG 日志打印宏
 * @note 定义宏来获取调用方元数据
 */
#define LOG(logger, level, format, ...) \
    logging(logger, level, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

/**
 * @brief LogLevel 日志输出的不同等级
 */
typedef enum LogLevel
{
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
} LogLevel;

/**
 * @brief Logger 日志记录器的结构体
 * @property file 日志文件
 * @property level 日志记录器等级
 */
typedef struct Logger
{
    FILE *file;
    LogLevel level;
} Logger;

/**
 * @brief 初始化日志记录器
 * @param file_name 日志文件名
 * @param level 日志记录器等级
 * @return Logger*
 */
Logger *init_logger(const char *file_name, LogLevel level);

/**
 * @brief 销毁日志记录器
 * @param logger 日志记录器的指针
 */
void destroy_logger(Logger *logger);

/**
 * @brief 记录日志
 * @param logger 日志记录器
 * @param level 日志等级
 * @param file 文件名 __FILE__
 * @param line 行号 __LINE__
 * @param function 函数名 __func__
 * @param format 格式
 */
void logging(Logger *logger, LogLevel level, const char *file, int line, const char *function, const char *format, ...);

extern Logger *logger;

#endif