#ifndef V4L2_CAPTURE_H
#define V4L2_CAPTURE_H

#include "ring_buffer.h"
#include <stddef.h>

typedef struct v4l2_capture_t {
    char device_path[256];  // 设备路径，如 "/dev/video0"
    int fd;                 // 设备文件描述符
    int width;              // 图像宽度
    int height;             // 图像高度
    int pixel_format;       // 像素格式（0=默认，如 V4L2_PIX_FMT_YUYV）
    void **buffers;         // mmap 映射的内核缓冲区指针数组
    int buffer_count;       // 缓冲区数量（通常 4 个）
    size_t buffer_size;      // 每个缓冲区的大小
    ring_buffer_t *rb;      // 关联的环形缓冲区
} v4l2_capture_t;

/* 初始化：打开设备、设置格式、mmap、关联 ring_buffer。成功返回 0 */
int  v4l2_init(v4l2_capture_t *cap, const char *device_path,
               int width, int height, ring_buffer_t *rb);
int  v4l2_start(v4l2_capture_t *cap);    // 开始采集，成功返回 0
int  v4l2_stop(v4l2_capture_t *cap);     // 停止采集，成功返回 0
void v4l2_destroy(v4l2_capture_t *cap);  // 释放资源
#endif /* V4L2_CAPTURE_H */