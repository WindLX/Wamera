#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev2.h>

// 用户层缓冲区
typedef struct
{
    void *start;
    int length;
} BufType;

BufType *usr_buf;

unsigned int buf_num = 4;

int camera_fd;

/**
 * @brief init_camera 初始化相机设备属性
 * @param dev 设备名称
 * @return 成功0, 错误-1
 */
int init_camera(const char *dev)
{
    camera_fd = open(dev, O_RDWR);
    if (camera_fd < 0)
    {
        printf("[ERROR]Open `%s` falied\n", dev);
        return -1;
    }

    // 查询设备属性
    struct v4l2_capability cap;
    int ret = ioctl(camera_fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0)
    {
        printf("[Error]VIDIOC_QUERYCAP failed\n");
        return -1;
    }

    printf("[INFO]Driver: %s\n", cap.driver);
    printf("[INFO]Device: %s\n", cap.card);
    printf("[INFO]Bus: %s\n", cap.bus_info);
    printf("[INFO]Version: %d\n", cap.version);

    // 判断是否为视频捕获设备
    if (cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        // 判断是否支持视频流
        if (cap.capabilities & V4L2_CAP_STREAMING)
        {
            printf("[INFO]Support capture\n");
        }
        else
        {
            printf("[INFO]Unsupport capture\n");
        }
    }
    else
    {
        printf("[ERROR]Not video capture\n");
        return -1;
    }

    // 获取可用格式
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("[INFO]Format supported\n");
    while (ioctl(camera_fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        printf("[INFO]%d %s\n", fmtdesc.index + 1, fmtdesc.description);
        struct v4l2_frmsizeenum frmsize;
        frmsize.index = 0;
        frmsize.pixel_format = fmtdesc.pixelformat;

        while (ioctl(camera_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
        {
            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                printf("[INFO]\t%d. Width: %u, Height: %u\n",
                       frmsize.index + 1,
                       frmsize.discrete.width,
                       frmsize.discrete.height);
            }
            frmsize.index++;
        }
        fmtdesc.index++;
    }

    // 设置帧格式
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1920;
    fmt.fmt.pix.height = 1080;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    if (ioctl(camera_fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        printf("[ERROR]set foramt:V4L2_PIX_FMT_MJPEG failed\n");
        return -1;
    }

    // TODO 设置帧速率
    return 0;
}

/**
 * @brief mmap_buffer 分配用户缓冲区内存,并建立内存映射
 * @return 成功返回0，失败返回-1
 */
int mmap_buffer()
{
    usr_buf = (BufType *)calloc(buf_num, sizeof(BufType));
    if (!usr_buf)
    {
        printf("[ERROR]calloc `frame buffer` falied : Out of memory\n");
        return -1;
    }

    struct v4l2_requestbuffers req;
    req.count = buf_num;                    // 帧缓冲数量
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 视频捕获缓冲区类型
    req.memory = V4L2_MEMORY_MMAP;          // 内存映射方式
    if (ioctl(camera_fd, VIDIOC_REQBUFS, &req) < 0)
    {
        printf("[ERROR]VIDIOC_REQBUFS fail\n");
        return -1;
    }

    /*映射内核缓存区到用户空间缓冲区*/
    for (unsigned int i = 0; i < buf_num; ++i)
    {
        /*查询内核缓冲区信息*/
        struct v4l2_buffer v4l2_buf;
        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        v4l2_buf.index = i;
        if (ioctl(camera_fd, VIDIOC_QUERYBUF, &v4l2_buf) < 0)
        {
            printf("[ERROR]VIDIOC_QUERYBUF failed\n");
            return -1;
        }

        /* 建立映射关系
         * 注意这里的索引号，v4l2_buf.index 与 usr_buf 的索引是一一对应的,
         * 当我们将内核缓冲区出队时，可以通过查询内核缓冲区的索引来获取用户缓冲区的索引号，
         * 进而能够知道应该在第几个用户缓冲区中取数据
         */
        usr_buf[i].length = v4l2_buf.length;
        usr_buf[i].start = (char *)mmap(0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera_fd, v4l2_buf.m.offset);

        if (MAP_FAILED == usr_buf[i].start)
        {
            // 若映射失败,打印错误
            printf("[ERROR]mmap failed: %d\n", i);
            return -1;
        }
        else
        {
            if (ioctl(camera_fd, VIDIOC_QBUF, &v4l2_buf) < 0)
            {
                // 若映射成功则将内核缓冲区入队
                printf("[ERROR]VIDIOC_QBUF failed\n");
                return -1;
            }
        }
    }
    return 0;
}

/**
 * @brief stream_on 开启视频流
 * @return 成功返回0，失败返回-1
 */
int stream_on()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_fd, VIDIOC_STREAMON, &type) < 0)
    {
        printf("[ERROR]VIDIOC_STREAMON failed\n");
        return -1;
    }
    return 0;
}

/**
 * @brief write_frame 读取一帧图像
 * @return  返回图像帧的索引index,读取失败返回-1
 */
int write_frame()
{
    struct v4l2_buffer v4l2_buf;
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(camera_fd, VIDIOC_DQBUF, &v4l2_buf) < 0) // 内核缓冲区出队列
    {
        printf("[ERROR]VIDIOC_DQBUF failed, dropped frame\n");
        return -1;
    }

    /*
     * 因为内核缓冲区与用户缓冲区建立的映射，所以可以通过用户空间缓冲区直接访问这个缓冲区的数据
     */
    char buffer[256];
    sprintf(buffer, "/home/windlx/Work/Complex/Wamera/video/%d.jpg", v4l2_buf.index);
    int file_fd = open(buffer, O_RDWR | O_CREAT, 0666); // 若打开失败则不存储该帧图像
    if (file_fd > 0)
    {
        printf("[INFO]saving %d images\n", v4l2_buf.index);
        write(file_fd, usr_buf[v4l2_buf.index].start, usr_buf[v4l2_buf.index].length);
        close(file_fd);
    }

    if (ioctl(camera_fd, VIDIOC_QBUF, &v4l2_buf) < 0) // 缓冲区重新入队
    {
        printf("[ERROR]VIDIOC_QBUF failed, dropped frame\n");
        return -1;
    }
    return v4l2_buf.index;
}

/**
 * @brief stream_off 关闭视频流
 * @return 成功返回0,失败返回-1
 */
int stream_off()
{
    /*关闭视频流*/
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        printf("[ERROR]ioctl `VIDIOC_STREAMOFF` failed");
        return -1;
    }
    return 0;
}

/**
 * @brief unmap_buffer 解除缓冲区映射
 * @return 成功返回0,失败返回-1
 */
int unmap_buffer()
{
    /*解除内核缓冲区到用户缓冲区的映射*/
    for (unsigned int i = 0; i < buf_num; i++)
    {
        int ret = munmap(usr_buf[i].start, usr_buf[i].length);
        if (ret < 0)
        {
            printf("[ERROR]munmap failed\n");
            return -1;
        }
    }
    free(usr_buf); // 释放用户缓冲区内存
    return 0;
}

/**
 * @brief release_camera 关闭设备
 */
void release_camera()
{
    close(camera_fd);
}

int main()
{
    int ret = init_camera("/dev/video2");
    if (ret < 0)
    {
        printf("init_camera error\n");
        return -1;
    }

    ret = mmap_buffer();
    if (ret < 0)
    {
        printf("mmap_buffer error\n");
        return -1;
    }

    ret = stream_on();
    if (ret < 0)
    {
        printf("stream_on error\n");
        return -1;
    }

    for (int i = 0; i < 30; i++)
    {
        write_frame();
    }

    ret = stream_off();
    if (ret < 0)
    {
        printf("stream_off error\n");
        return -1;
    }

    ret = unmap_buffer();
    if (ret < 0)
    {
        printf("unmap_buffer error\n");
        return -1;
    }

    release_camera();
}