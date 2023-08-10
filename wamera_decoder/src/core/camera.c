#include "../../include/camera.h"

Camera *init_camera(const char *dev)
{
    Camera *camera = (Camera *)malloc(sizeof(Camera));

    camera->fd = open(dev, O_RDWR);
    if (camera->fd < 0)
    {
        LOG(logger, LOG_ERROR, "Open camera failed: `%s`", dev);
        free(camera);
        return NULL;
    }
    LOG(logger, LOG_INFO, "Open camera successfully: `%s`", dev);

    // 查询设备属性
    struct v4l2_capability cap;
    if (ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        LOG(logger, LOG_ERROR, "Query capability failed: `%s`", dev);
        close(camera->fd);
        free(camera);
        return NULL;
    }

    LOG(logger, LOG_DEBUG,
        "`%s`:\n\
            \tDriver: %s\n\
            \tDevice: %s\n\
            \tBus: %s\n\
            \tVersion: %d",
        dev, cap.driver, cap.card, cap.bus_info, cap.version);

    // 判断是否为视频捕获设备
    if (cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        // 判断是否支持视频流
        if (cap.capabilities != V4L2_CAP_STREAMING)
            LOG(logger, LOG_INFO, "`%s` unsupport capture", dev);
    }
    else
    {
        LOG(logger, LOG_ERROR, "`%s` is not a video capture", dev);
        close(camera->fd);
        free(camera);
        return NULL;
    }

    return camera;
}

Config get_config(Camera *camera)
{
    Config config = {0, 0, MJPEG, {1, 1}, 0};

    struct v4l2_format fmt;
    if (ioctl(camera->fd, VIDIOC_G_FMT, &fmt) < 0)
        LOG(logger, LOG_WARNING, "Get format failed");
    else
    {
        config.height = fmt.fmt.pix.height;
        config.width = fmt.fmt.pix.width;
        if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
            config.pix_format = YUYV;
        else
            config.pix_format = MJPEG;
    }

    struct v4l2_streamparm stream_params;
    memset(&stream_params, 0, sizeof(stream_params));
    stream_params.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_G_PARM, &stream_params) < 0)
        LOG(logger, LOG_WARNING, "Get stream parameters failed");
    else
    {
        config.time_base = (AVRational){
            stream_params.parm.capture.timeperframe.numerator,
            stream_params.parm.capture.timeperframe.denominator,
        };
        double framerate = av_q2d(av_inv_q(config.time_base));
        LOG(logger, LOG_DEBUG, "Frame rate: %.2f fps\n", framerate);
    }

    return config;
}

LinkedList *get_available_configs(Camera *camera)
{
    LinkedList *available_configs = create_linked_list();

    LOG(logger, LOG_DEBUG, "Format supported:");

    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(camera->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        LOG(logger, LOG_DEBUG, "%d %s %s", fmtdesc.index + 1, fmtdesc.description, (char *)&fmtdesc.pixelformat);

        struct v4l2_frmsizeenum frmsize;
        frmsize.index = 0;
        frmsize.pixel_format = fmtdesc.pixelformat;
        while (ioctl(camera->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                LOG(logger, LOG_DEBUG,
                    "\t%d. Width: %u, Height: %u",
                    frmsize.index + 1, frmsize.discrete.width, frmsize.discrete.height);
                PixFormat pfrm = (fmtdesc.pixelformat == V4L2_PIX_FMT_MJPEG) ? MJPEG : YUYV;
                Config config = {
                    frmsize.discrete.width,
                    frmsize.discrete.height,
                    pfrm,
                    {1, 1},
                    0};
                Config *config_copy = (Config *)malloc(sizeof(Config));
                if (config_copy)
                {
                    memcpy(config_copy, &config, sizeof(Config));
                    append_linked_list(available_configs, (void *)config_copy);
                }
                else
                    LOG(logger, LOG_ERROR, "Memory allocation failed");
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    return available_configs;
}

int set_camera_config(Camera *camera, Config config)
{
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config.width;
    fmt.fmt.pix.height = config.height;
    if (config.pix_format == YUYV)
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    else
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if (ioctl(camera->fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        LOG(logger, LOG_WARNING, "Set format failed");
        return -1;
    }

    struct v4l2_streamparm streamParams;
    memset(&streamParams, 0, sizeof(streamParams));
    streamParams.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamParams.parm.capture.timeperframe.numerator = config.time_base.num;
    streamParams.parm.capture.timeperframe.denominator = config.time_base.den;
    if (ioctl(camera->fd, VIDIOC_S_PARM, &streamParams) < 0)
    {
        LOG(logger, LOG_WARNING, "Set format failed");
        return -1;
    }
    return 0;
}

/**
 * @brief mmap_buffer 分配用户缓冲区内存,并建立内存映射
 * @return 成功返回0，失败返回-1
 */
int mmap_buffer(Camera *camera)
{
    camera->usr_buf = (BufType *)calloc(BUF_NUM, sizeof(BufType));
    if (!camera->usr_buf)
    {
        LOG(logger, LOG_ERROR, "Calloc `frame buffer` failed: Out of memory");
        return -1;
    }

    struct v4l2_requestbuffers req;
    req.count = BUF_NUM;                    // 帧缓冲数量
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 视频捕获缓冲区类型
    req.memory = V4L2_MEMORY_MMAP;          // 内存映射方式
    if (ioctl(camera->fd, VIDIOC_REQBUFS, &req) < 0)
    {
        LOG(logger, LOG_ERROR, "Request buffer failed");
        return -1;
    }

    /*映射内核缓存区到用户空间缓冲区*/
    for (unsigned int i = 0; i < BUF_NUM; ++i)
    {
        /*查询内核缓冲区信息*/
        struct v4l2_buffer v4l2_buf;
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        v4l2_buf.index = i;
        if (ioctl(camera->fd, VIDIOC_QUERYBUF, &v4l2_buf) < 0)
        {
            LOG(logger, LOG_ERROR, "Query buffer failed");
            return -1;
        }

        /* 建立映射关系
         * 注意这里的索引号，v4l2_buf.index 与 usr_buf 的索引是一一对应的,
         * 当我们将内核缓冲区出队时，可以通过查询内核缓冲区的索引来获取用户缓冲区的索引号，
         * 进而能够知道应该在第几个用户缓冲区中取数据
         */
        camera->usr_buf[i].length = v4l2_buf.length;
        camera->usr_buf[i].start = (char *)mmap(0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera->fd, v4l2_buf.m.offset);
        if (MAP_FAILED == camera->usr_buf[i].start)
        {
            // 若映射失败,打印错误
            LOG(logger, LOG_ERROR, "Mmap at index `%d` failed", i);
            return -1;
        }
        else
        {
            if (ioctl(camera->fd, VIDIOC_QBUF, &v4l2_buf) < 0)
            {
                // 若映射成功则将内核缓冲区入队
                LOG(logger, LOG_ERROR, "VIDIOC_QBUF at index `%d` failed", i);
                return -1;
            }
        }
    }
    return 0;
}

/**
 * @brief open_stream 开启视频流
 * @return 成功返回0，失败返回-1
 */
int open_stream(Camera *camera)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMON, &type) < 0)
    {
        LOG(logger, LOG_ERROR, "Open stream failed");
        return -1;
    }
    return 0;
}

/**
 * @brief close_stream 关闭视频流
 * @return 成功返回0, 失败返回-1
 */
int close_stream(Camera *camera)
{
    /*关闭视频流*/
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        LOG(logger, LOG_ERROR, "Close stream failed");
        return -1;
    }
    return 0;
}

/**
 * @brief munmap_buffer 解除缓冲区映射
 * @return 成功返回0,失败返回-1
 */
int munmap_buffer(Camera *camera)
{
    /*解除内核缓冲区到用户缓冲区的映射*/
    for (unsigned int i = 0; i < BUF_NUM; i++)
    {
        int ret = munmap(camera->usr_buf[i].start, camera->usr_buf[i].length);
        if (ret < 0)
        {
            LOG(logger, LOG_ERROR, "Munmap failed");
            return -1;
        }
    }
    free(camera->usr_buf); // 释放用户缓冲区内存
    return 0;
}

int open_camera(Camera *camera)
{
    int ret = mmap_buffer(camera) | open_stream(camera);
    if (ret < 0)
        LOG(logger, LOG_ERROR, "Open camera failed");
    else
        LOG(logger, LOG_DEBUG, "Open camera successfully");
    return ret;
}

int close_camera(Camera *camera)
{
    int ret = close_stream(camera) | munmap_buffer(camera);
    if (ret < 0)
        LOG(logger, LOG_ERROR, "Close camera failed");
    else
        LOG(logger, LOG_INFO, "Close camera successfully");
    return ret;
}

void destroy_camera(Camera *camera)
{
    close(camera->fd);
    free(camera);
    LOG(logger, LOG_INFO, "Destroy camera successfully");
}

BufType *get_frame(Camera *camera)
{
    struct v4l2_buffer v4l2_buf;
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(camera->fd, VIDIOC_DQBUF, &v4l2_buf) < 0) // 内核缓冲区出队列
    {
        LOG(logger, LOG_ERROR, "Failed to destroy camera");
        return NULL;
    }

    /*
     * 内核缓冲区与用户缓冲区建立的映射，通过用户空间缓冲区直接访问这个缓冲区的数据
     */
    BufType *frame_data = (BufType *)malloc(sizeof(BufType));
    if (frame_data == NULL)
    {
        LOG(logger, LOG_ERROR, "Memory allocation failed");
        return NULL;
    }

    frame_data->start = malloc(camera->usr_buf[v4l2_buf.index].length);
    if (frame_data->start == NULL)
    {
        LOG(logger, LOG_ERROR, "Memory allocation failed");
        free(frame_data);
        return NULL;
    }

    frame_data->length = camera->usr_buf[v4l2_buf.index].length;

    memcpy(frame_data->start, camera->usr_buf[v4l2_buf.index].start,
           camera->usr_buf[v4l2_buf.index].length);

    if (ioctl(camera->fd, VIDIOC_QBUF, &v4l2_buf) < 0) // 缓冲区重新入队
    {
        LOG(logger, LOG_ERROR, "Failed to VIDIOC_QBUF, dropped frame");
        return NULL;
    }
    return frame_data;
}