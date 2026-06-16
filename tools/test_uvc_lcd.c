#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <signal.h>

#define V4L2_DEV    "/dev/video1"
#define FB_DEV      "/dev/fb0"
#define CAP_WIDTH   640
#define CAP_HEIGHT  480
#define BUF_COUNT   4

static volatile int running = 1;

static void sigint_handler(int sig) { running = 0; }// 信号处理函数，当接收到 SIGINT 信号时，将 running 变量设置为 0，通知主循环退出

/*
 * YUYV to RGB32 (24-bit color in 32-bit pixel).
 * red_off/blue_off from fb_var_screeninfo determine byte layout.
 *   XRGB8888 → red_off=16, green_off=8,  blue_off=0
 *   XBGR8888 → red_off=0,  green_off=8,  blue_off=16
 */
static void yuyv_to_rgb32(const unsigned char *yuyv, unsigned char *rgb,
                          int width, int height,
                          int roff, int goff, int boff) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int Y0 = yuyv[0], U  = yuyv[1];
            int Y1 = yuyv[2], V  = yuyv[3];
            yuyv += 4;

            int C = Y0 - 16,  D = U - 128, E = V - 128;
            int r = (298 * C + 409 * E + 128) >> 8;
            int g = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int b = (298 * C + 516 * D + 128) >> 8;
#define CLAMP(v) ((v) < 0 ? 0 : (v) > 255 ? 255 : (v))
            r = CLAMP(r); g = CLAMP(g); b = CLAMP(b);

            rgb[roff] = r; rgb[goff] = g; rgb[boff] = b; rgb += 4;

            C = Y1 - 16;
            r = (298 * C + 409 * E + 128) >> 8;
            g = (298 * C - 100 * D - 208 * E + 128) >> 8;
            b = (298 * C + 516 * D + 128) >> 8;
            r = CLAMP(r); g = CLAMP(g); b = CLAMP(b);

            rgb[roff] = r; rgb[goff] = g; rgb[boff] = b; rgb += 4;
        }
    }
#undef CLAMP
}

int main() {
    signal(SIGINT, sigint_handler);

    /* -------- 初始化摄像头 -------- */
    int vfd = open(V4L2_DEV, O_RDWR);
    if (vfd < 0) { perror("open v4l2"); return 1; }

    struct v4l2_capability cap;
    ioctl(vfd, VIDIOC_QUERYCAP, &cap);
    printf("Camera: %s | %s\n", cap.driver, cap.card);

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width  = CAP_WIDTH;
    fmt.fmt.pix.height = CAP_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(vfd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT"); return 1;
    }
    printf("Capture: %dx%d YUYV\n", fmt.fmt.pix.width, fmt.fmt.pix.height);

    struct v4l2_requestbuffers req = {0};
    req.count  = BUF_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(vfd, VIDIOC_REQBUFS, &req);

    struct { void *start; size_t len; } vbufs[BUF_COUNT];
    for (int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        ioctl(vfd, VIDIOC_QUERYBUF, &buf);
        vbufs[i].len   = buf.length;
        vbufs[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, vfd, buf.m.offset);
        ioctl(vfd, VIDIOC_QBUF, &buf);
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(vfd, VIDIOC_STREAMON, &type);

    /* -------- 初始化 LCD -------- */
    int fbfd = open(FB_DEV, O_RDWR);
    if (fbfd < 0) { perror("open fb0"); return 1; }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    printf("LCD: %dx%d, %dbpp, line_len=%d\n",
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);

    size_t fb_size = finfo.line_length * vinfo.yres;
    unsigned char *fb = mmap(NULL, fb_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fbfd, 0);

    int roff = vinfo.red.offset   / 8;
    int goff = vinfo.green.offset / 8;
    int boff = vinfo.blue.offset  / 8;
    printf("RGB byte offsets: R=%d G=%d B=%d\n", roff, goff, boff);

    /* -------- 主循环：采集 → 转码 → 上屏 -------- */
    struct timeval start, now;
    gettimeofday(&start, NULL);
    unsigned int frames = 0;

    while (running) {
        struct v4l2_buffer buf = {0};
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(vfd, VIDIOC_DQBUF, &buf) < 0) break;

        // 转 YUYV → RGB32
        unsigned char *rgb = malloc(CAP_WIDTH * CAP_HEIGHT * 4);
        yuyv_to_rgb32(vbufs[buf.index].start, rgb,
                      CAP_WIDTH, CAP_HEIGHT, roff, goff, boff);

        // 逐行拷贝到 fb0（摄像头 480 行放在屏幕上边缘）
        int fb_stride = finfo.line_length;
        int rgb_stride = CAP_WIDTH * 4;
        unsigned char *dst = fb;
        unsigned char *src = rgb;
        for (int row = 0; row < CAP_HEIGHT; row++) {
            memcpy(dst, src, rgb_stride);
            dst += fb_stride;
            src += rgb_stride;
        }

        free(rgb);
        frames++;

        ioctl(vfd, VIDIOC_QBUF, &buf);

        if (frames % 30 == 0) {
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - start.tv_sec) +
                            (now.tv_usec - start.tv_usec) / 1000000.0;
            printf("FPS: %.1f\n", frames / elapsed);
        }
    }

    ioctl(vfd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < req.count; i++) munmap(vbufs[i].start, vbufs[i].len);
    munmap(fb, fb_size);
    close(vfd);
    close(fbfd);
    printf("Done. %u frames total.\n", frames);
    return 0;
}
