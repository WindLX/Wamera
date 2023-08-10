#ifndef CAMERA_H
#define CMAERA_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev2.h>
#include <libavutil/mathematics.h>

#include "./logger.h"
#include "./tool.h"

// 用户层缓冲区大小
#define BUF_NUM 4

/**
 * @brief 相机
 * @property fd 设备索引号
 * @property config 相机配置信息
 * @property user_buf 用户层缓冲区
 */
typedef struct Camera
{
    int fd;
    BufType *usr_buf;
} Camera;

/**
 * @brief init_camera 初始化相机设备属性
 * @param dev 设备名称
 * @return Camera*
 */
Camera *init_camera(const char *dev);

/**
 * @brief open_camera 开启相机
 * @param camera 相机设备
 * @return 设置成功返回0, 失败返回-1
 */
int open_camera(Camera *camera);

/**
 * @brief set_camera_config 对相机进行设置
 * @param camera 相机设备
 * @param config 配置信息
 * @param int 设置成功返回0, 失败返回-1
 */
int set_camera_config(Camera *camera, Config config);

/**
 * @brief get_config 获取相机当前设置
 * @param camera 相机设备
 * @return Config 设置信息
 */
Config get_config(Camera *camera);

/**
 * @brief get_available_configs 获取相机所有可用设置
 * @param camera 相机设备
 * @return LinkedList* 设置信息
 */
LinkedList *get_available_configs(Camera *camera);

/**
 * @brief get_frame 读取一帧图像
 * @param camera 相机设备
 * @return BufType* 返回图像帧的指针
 */
BufType *get_frame(Camera *camera);

/**
 * @brief close_camera 关闭设备
 * @return 关闭成功返回0, 失败返回-1
 */
int close_camera(Camera *camera);

/**
 * @brief destroy_camera 注销设备
 */
void destroy_camera(Camera *camera);

#endif