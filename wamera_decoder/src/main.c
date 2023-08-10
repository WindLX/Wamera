#include "../include/logger.h"
#include "../include/camera.h"
#include "../include/codec.h"
#include "../include/tool.h"

int main()
{
    Config config = {1920, 1080, MJPEG, {1, 30}, 3600, 500000};

    logger = init_logger("./log/test.log", LOG_DEBUG);
    Camera *camera = init_camera("/dev/video2");

    if (set_camera_config(camera, config) < 0)
        LOG(logger, LOG_WARNING, "Set camera config failed");
    if (open_camera(camera) < 0)
        exit(-1);

    Codec *codec = init_codec(LOG_INFO);
    if (open_codec(codec, config))
        exit(-1);

    Output *rtmp_output = open_output(config, "rtmp://0.0.0.0:1935/wd_video/123", "flv");
    Output *file_output = NULL;
    const unsigned int num = 2;
    Output *output[num];
    unsigned int count = 0;
    LOG(logger, LOG_INFO, "Start push stream");

    while (1)
    {
        if (count % get_save_frame(config) == 0)
        {
            time_t rawtime;
            struct tm *timeinfo;
            char timestamp[20];
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
            char path[80];
            sprintf(path, "/home/windlx/Work/Complex/Wamera/video/out_%s.mp4", timestamp);
            file_output = open_output(config, path, "mp4");
            LOG(logger, LOG_INFO, "Start write file: %s", path);
        }
        BufType *frame = get_frame(camera);
        output[0] = rtmp_output;
        output[1] = file_output;
        if (dispose_codec(codec, output, num, *frame, count) == -1)
            break;
        if (count % get_save_frame(config) == get_save_frame(config) - 1)
        {
            close_output(file_output);
        }
        count++;
    }

    if (close_output(rtmp_output) < 0)
        exit(-1);
    if (file_output)
        if (close_output(file_output) < 0)
            exit(-1);
    LOG(logger, LOG_INFO, "End push stream");
    close_codec(codec);
    destroy_codec(codec);
    close_camera(camera);
    destroy_camera(camera);
    LOG(logger, LOG_INFO, "Close camera");
    destroy_logger(logger);
}