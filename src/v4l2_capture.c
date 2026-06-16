#include "v4l2_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
/*
1. 打开设备 —open(device_path, O_RDWR)，记得检查返回值喵
2. 查询能力 —ioctl(fd, VIDIOC_QUERYCAP, &cap)，确认这是一个 V4L2_CAP_VIDEO_CAPTURE 设备，且支持 V4L2_CAP_STREAMING 喵
3. 设置格式 —VIDIOC_S_FMT，fmt.fmt.pix 里填好 width=640、height=480、pixelformat=V4L2_PIX_FMT_YUYV喵
4. 申请缓冲区 —VIDIOC_REQBUFS，申请 4 个 buffer（type=V4L2_BUF_TYPE_VIDEO_CAPTURE, memory=V4L2_MEMORY_MMAP）喵
5. mmap 映射 —循环 count 次，每帧用 VIDIOC_QUERYBUF 拿到 offset 和 length →mmap(NULL, buf.length,
PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset) →存入 cap->buffers[i]喵
6. 入队 —每个 buffer 都 VIDIOC_QBUF 一次，准备接收数据喵
*/
static void cleanup_mmap(v4l2_capture_t *cap,int i){
    for (int j = 0; j < i; j++) {
        if (cap->buffers[j]) {
            munmap(cap->buffers[j], cap->buffer_size);
        }
    }
}
int v4l2_init(v4l2_capture_t *cap, const char *device_path,
               int width, int height, ring_buffer_t *rb) {
    // 1. 打开设备
    cap->fd = open(device_path, O_RDWR);
    if (cap->fd < 0) {
        perror("Failed to open device");
        return -1;
    }
    strncpy(cap->device_path, device_path, sizeof(cap->device_path) - 1);
    cap->width = width;
    cap->height = height;
    cap->pixel_format = V4L2_PIX_FMT_YUYV; // 默认像素格式
    cap->rb = rb;

    // 2. 查询能力
    struct v4l2_capability cap_info;
    if (ioctl(cap->fd, VIDIOC_QUERYCAP, &cap_info) < 0) {
        perror("Failed to query capabilities");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }
    if (!(cap_info.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }
    if (!(cap_info.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }

    // 3. 设置格式
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = cap->pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE; // 无交错
    if (ioctl(cap->fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("Failed to set format");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }
    cap->width = fmt.fmt.pix.width; // 可能被调整了
    cap->height = fmt.fmt.pix.height;

    // 4. 申请缓冲区
    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));// 清零,以免 ioctl 里有垃圾数据
    reqbuf.count = 4; // 请求 4 个缓冲区
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;// 使用 mmap 模式
    if (ioctl(cap->fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        perror("Failed to request buffers");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }
    cap->buffer_count = reqbuf.count;
    cap->buffers = calloc(cap->buffer_count, sizeof(void *));// 分配指针数组,每个指针指向一个缓冲区
    if (!cap->buffers) {
        fprintf(stderr, "Failed to allocate buffer pointers\n");
        close(cap->fd);
        cap->fd = -1;
        return -1;
    }
    // 5. mmap 映射
    for (int i = 0; i < cap->buffer_count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(cap->fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("Failed to query buffer");
            cleanup_mmap(cap, i); // 释放已映射的缓冲区
            free(cap->buffers);
            close(cap->fd);
            cap->buffers = NULL;
            cap->buffer_count = 0;
            cap->fd = -1;
            return -1;
        }
        cap->buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, cap->fd, buf.m.offset);
        cap->buffer_size = buf.length;
        if (cap->buffers[i] == MAP_FAILED) {
            perror("Failed to mmap buffer");
            cleanup_mmap(cap, i); // 释放已映射的缓冲区
            free(cap->buffers);
            close(cap->fd);
            cap->buffers = NULL;
            cap->buffer_count = 0;
            cap->fd = -1;
            return -1;
        }
    }

    // 6. 入队
    for (int i = 0; i < cap->buffer_count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(cap->fd, VIDIOC_QBUF, &buf) < 0) {
            cleanup_mmap(cap, cap->buffer_count); // 释放所有映射的缓冲区
            perror("Failed to queue buffer");
            free(cap->buffers);
            close(cap->fd);
            cap->buffers = NULL;
            cap->buffer_count = 0;
            cap->fd = -1;
            return -1;
        }
    }
    return 0; // 成功
}

int v4l2_start(v4l2_capture_t *cap) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cap->fd, VIDIOC_STREAMON, &type) < 0) {
        perror("Failed to start streaming");
        return -1;
    }
    return 0; // 成功
}

int v4l2_stop(v4l2_capture_t *cap) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cap->fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("Failed to stop streaming");
        return -1;
    }
    return 0; // 成功
}

void v4l2_destroy(v4l2_capture_t *cap) {
    // 释放 mmap 映射的缓冲区
    if(cap->fd >= 0){
        v4l2_stop(cap); // 确保停止采集
        }
    for (int i = 0; i < cap->buffer_count; i++) {
        if (cap->buffers[i]) {
            munmap(cap->buffers[i], cap->buffer_size);
        }
    }
    free(cap->buffers);
    close(cap->fd);
    cap->buffers = NULL;
    cap->buffer_count = 0;
    cap->fd = -1;
}