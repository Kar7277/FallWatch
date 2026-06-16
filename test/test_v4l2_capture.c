#include "v4l2_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <poll.h>

#define DQBUF_TIMEOUT_MS 2000  /* DQBUF 超时时间：2秒，摄像头异常时避免永久阻塞 */

int main(void) {
    v4l2_capture_t cap = {0}; // 初始化结构体为全零
    ring_buffer_t *rb = ring_buffer_init(8);

    /* 初始化环形缓冲区（测试中暂时不实际使用，但 v4l2_init 需要它） */
    if (!rb) {
        fprintf(stderr, "Failed to init ring buffer\n");
        return EXIT_FAILURE;
    }

    /* ====== 测试1：初始化设备 ====== */
    if (v4l2_init(&cap, "/dev/video1", 640, 480, rb) != 0) {
        fprintf(stderr, "FAIL: v4l2_init\n");
        ring_buffer_destroy(rb);
        return EXIT_FAILURE;
    }
    printf("[PASS] Test 1: v4l2_init\n");

    /* ====== 测试2：启动采集 ====== */
    if (v4l2_start(&cap) != 0) {
        fprintf(stderr, "FAIL: v4l2_start\n");
        v4l2_destroy(&cap);
        ring_buffer_destroy(rb);
        return EXIT_FAILURE;
    }
    printf("[PASS] Test 2: v4l2_start\n");

    /* ====== 测试3：采集10帧 ====== */
    int captured = 0;
    const int target = 10;// 目标采集帧数,const修饰符告诉编译器这个值不会改变，允许它进行优化，比如直接把 target 替换成 10，避免每次循环都访问内存读取 target 的值，提高性能。
    for (int i = 0; i < target; i++) {
        /* 3a. 准备 v4l2_buffer 结构体 */
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));           // 必须清零，防止垃圾数据引发 ioctl 错误
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 告诉驱动：操作的是视频采集 buffer
        buf.memory = V4L2_MEMORY_MMAP;             // 告诉驱动：用的是 mmap 模式

        /* 3b. poll() 等待数据就绪，超时则判定摄像头异常 */
        struct pollfd fds[1];
        fds[0].fd = cap.fd;
        fds[0].events = POLLIN;                // 关心"有数据可读"
        int ret = poll(fds, 1, DQBUF_TIMEOUT_MS);
        if (ret == 0) {
            fprintf(stderr, "DQBUF timeout: camera may be disconnected or stalled\n");
            break;
        }
        if (ret < 0) {
            perror("poll failed");
            break;
        }

        /* 3c. DQBUF：poll 确认可读后才调，不会阻塞 */
        if (ioctl(cap.fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("DQBUF failed");
            break;  // 取帧失败则提前结束
        }
        /* 3d. 验证帧数据是否有效：bytesused 是驱动实际写入的字节数 */
        if (buf.bytesused > 0) {
            captured++;
            printf("  Frame %2d: buf[%d] %u bytes\n",
                   i + 1, buf.index, buf.bytesused);
        } else {
            printf("  Frame %2d: EMPTY (0 bytes)\n", i + 1);
        }

        /* 3e. QBUF：把 buffer 归还驱动的"待填充队列"，不还的话跑几轮就没 buffer 可用了 */
        if (ioctl(cap.fd, VIDIOC_QBUF, &buf) < 0) {
            perror("QBUF failed");
            break;
        }
    }
    if (captured == target) {
        printf("[PASS] Test 3: capture %d frames (%d/%d)\n", target, captured, target);
    } else {
        printf("[FAIL] Test 3: capture %d frames (%d/%d)\n", target, captured, target);
    }
    
    /* ====== 测试4：停止采集 ====== */
    if (v4l2_stop(&cap) != 0) {
        fprintf(stderr, "FAIL: v4l2_stop\n");
        v4l2_destroy(&cap);
        ring_buffer_destroy(rb);
        return EXIT_FAILURE;
    }
    printf("[PASS] Test 4: v4l2_stop\n");

    /* 清理资源 */
    v4l2_destroy(&cap);
    ring_buffer_destroy(rb);

    /* 总结 */
    printf("---\nAll 4 tests passed.\n");
    return EXIT_SUCCESS;
}
