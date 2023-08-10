#include "../../include/logger.h"

Logger *logger;

const char *level_to_string(LogLevel level)
{
    switch (level)
    {
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_INFO:
        return "INFO";
    case LOG_WARNING:
        return "WARNING";
    case LOG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

Logger *init_logger(const char *file_name, LogLevel level)
{
    Logger *logger = (Logger *)malloc(sizeof(Logger));
    logger->file = NULL;
    if (file_name)
    {
        logger->file = fopen(file_name, "a, ccs=UTF-8");
        if (!logger->file)
        {
            printf("Failed to find log file\n");
        }
    }
    logger->level = level;
    return logger;
}

void destroy_logger(Logger *logger)
{
    if (logger->file)
    {
        fclose(logger->file);
    }
    free(logger);
}

void logging(Logger *logger, LogLevel level, const char *file, int line, const char *function, const char *format, ...)
{
    if (level < logger->level)
        return;

    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    va_list args;
    va_start(args, format);

    printf("[%s] [%s] [%s:%d] %s: ", timestamp, level_to_string(level), file, line, function);
    vprintf(format, args);
    printf("\n");

    va_end(args);           // 复位 args 参数
    va_start(args, format); // 重新初始化参数列表

    if (logger->file && level != LOG_DEBUG)
    {
        fprintf(logger->file, "[%s] [%s] [%s:%d] %s: ", timestamp, level_to_string(level), file, line, function);
        vfprintf(logger->file, format, args);
        fprintf(logger->file, "\n");
        fflush(logger->file);
    }

    va_end(args);
}