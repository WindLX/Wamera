#ifndef TOOL_H
#define TOOL_H

#include <stdlib.h>

#include <libavutil/mathematics.h>

#include "./logger.h"

#pragma region 用户层缓冲区

// 用户层缓冲区
typedef struct BufType
{
    void *start;
    int length;
} BufType;

/**
 * @brief 释放用户缓冲区
 * @param buf 释放区域的指针
 */
void destroy_buf(BufType *buf);

#pragma endregion

#pragma region 配置

// 像素格式
typedef enum PixFormat
{
    MJPEG = 0,
    YUYV = 1,
} PixFormat;

// 配置信息
typedef struct Config
{
    unsigned int width;
    unsigned int height;
    PixFormat pix_format;
    AVRational time_base;
    unsigned int save_time;
} Config;

#pragma endregion

#pragma region 链表

typedef struct Node
{
    void *data;
    struct Node *next;
} Node;

typedef struct LinkedList
{
    Node *head;
} LinkedList;

/**
 * @brief create_linked_list 创建链表
 * @return LinkedList*
 */
LinkedList *create_linked_list();

/**
 * @brief free_linked_list 释放链表
 * @param list 待释放的链表
 */
void free_linked_list(LinkedList *list);

/**
 * @brief append_linked_list 向链表尾部添加元素
 * @param list 待添加的链表
 * @param data 数据
 */
void append_linked_list(LinkedList *list, void *data);

/**
 * @brief foreach_linked_list 遍历链表
 * @param list 待处理的链表
 * @param func 处理函数
 */
void foreach_linked_list(LinkedList *list, void (*func)(void *));

/**
 * @brief get_linked_list 按索引获取链表中的元素
 * @param index 索引
 * @return void* 数据
 */
void *get_linked_list(LinkedList *list, unsigned int index);

/**
 * @brief remove_linked_list 按索引移除链表中的元素
 * @param index 索引
 * @return int 索引越界错误 -1
 */
int remove_linked_list(LinkedList *list, unsigned int index);

#pragma endregion

#endif