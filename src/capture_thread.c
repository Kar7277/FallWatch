#include "capture_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

static void* capture_loop(void *arg){
    capture_thread_ctx *ctx = (capture_thread_ctx*)arg;

    while(ctx->running){
        /* 1. poll：等待帧就绪，即设备是否可读，超时 2000ms */
        struct pollfd fds[1];
        fds[0].fd = ctx->cap->fd;
        fds[0].events = POLLIN;
        int ret = poll(fds, 1, 2000);//等待设备可读，超时 2000ms
        if (ret == 0) {
            fprintf(stderr, "capture_loop: poll timeout, camera stalled?\n");
            break;
        }
        if (ret < 0) {
            perror("capture_loop: poll failed");
            break;
        }
        /*这里的&是位运算符，用于检查poll事件是否包含错误或挂起*/
        if (fds[0].revents & (POLLERR | POLLHUP)) {
            fprintf(stderr, "capture_loop: device error (POLLERR/POLLHUP)\n");
            break;
        }

        /* 2. DQBUF：取出填满的帧 */
        struct v4l2_buffer buf; //系统内部定义好了的结构体，包含了帧的相关信息
        memset(&buf, 0, sizeof(buf));//清零，避免垃圾数据干扰
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->cap->fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("capture_loop: DQBUF failed");
            break;
        }

        /* 3. 构造 frame_t 并拷贝数据 */
        frame_t *frame = (frame_t *)malloc(sizeof(frame_t));
        if (!frame) {
            fprintf(stderr, "capture_loop: malloc(frame_t) failed\n");
            break;
        }
        frame->data = (unsigned char *)malloc(buf.bytesused);
        if (!frame->data) {
            fprintf(stderr, "capture_loop: malloc(data) failed\n");
            free(frame);
            break;
        }
        memcpy(frame->data, ctx->cap->buffers[buf.index], buf.bytesused);
        frame->id        = ctx->frame_id++;
        frame->data_size = buf.bytesused;
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            frame->timestamp = (long long)tv.tv_sec * 1000LL
                             + tv.tv_usec / 1000LL;
        }

        /* 4. 塞进环形缓冲区 */
        ring_buffer_put(ctx->rb, frame);

        /* 5. QBUF：归还 buffer 给驱动 */
        if (ioctl(ctx->cap->fd, VIDIOC_QBUF, &buf) < 0) {
            perror("capture_loop: QBUF failed");
            break;
        }
    }
    return NULL;
}
void capture_thread_start(capture_thread_ctx *ctx){
    ctx->running = 1; // 设置运行标志
    pthread_create(&ctx->thread, NULL, capture_loop, ctx); // 创建线程，执行 capture_loop
}

void capture_thread_stop(capture_thread_ctx *ctx){
    ctx->running = 0; // 设置停止标志
    pthread_join(ctx->thread, NULL); // 等待线程结束
}
