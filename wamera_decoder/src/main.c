#include "../include/logger.h"
#include "../include/camera.h"
#include "../include/codec.h"
#include "../include/tool.h"

int main()
{
    Config config = {1920, 1080, MJPEG, {1, 30}, 60};

    logger = init_logger("./log/test.log", LOG_DEBUG);
    Camera *camera = init_camera("/dev/video0");

    set_camera_config(camera, config);
    open_camera(camera);
    Codec *codec = init_codec(LOG_INFO);
    open_codec(codec, config);

    // AVFormatContext *rtmp_ctx = NULL;
    AVFormatContext *file_ctx = NULL;
    // open_output(codec, config, &rtmp_ctx, "rtmp://0.0.0.0:1935/wd_video/123", "flv");

    unsigned int count = 0;
    while (1)
    {
        if (count % config.save_time == 0)
        {
            char path[50];
            sprintf(path, "/home/windlx/Work/Complex/Wamera/out_%d.mp4", (count / config.save_time));
            open_output(codec, config, &file_ctx, path, "mp4");
        }
        // AVFormatContext *frm_ctx[] = {
        // rtmp_ctx, file_ctx};
        BufType *frame = get_frame(camera);
        dispose_codec(codec, *frame, count, file_ctx, 1);
        if (count % config.save_time == 0 && count >= config.save_time)
        {
            close_output(file_ctx);
        }
        count++;
    }
    // avformat_alloc_output_context2(&out_ctx, NULL, NULL, "/home/windlx/Work/Complex/Wamera/output.mp4");
    // avformat_alloc_output_context2(&out_ctx, NULL, "flv", "rtmp://0.0.0.0:1935/wd_video/123");
    close_output(file_ctx);
    // close_output(rtmp_ctx);
    close_codec(codec);
    destroy_codec(codec);
    close_camera(camera);
    destroy_camera(camera);
    destroy_logger(logger);
}
// char buffer[256];
// sprintf(buffer, "/home/windlx/Work/Complex/Wamera/video/%d.jpg", i);
// int file_fd = open(buffer, O_RDWR | O_CREAT, 0666); // 若打开失败则不存储该帧图像
// if (file_fd > 0)
// {
//     printf("saving %d images\n", i);
//     write(file_fd, frame->start, frame->length);
//     close(file_fd);
// }