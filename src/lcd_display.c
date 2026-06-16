#include "lcd_display.h"
#include <fcntl.h>       // open, O_RDWR
#include <unistd.h>      // close
#include <sys/ioctl.h>   // ioctl
#include <sys/mman.h>    // mmap, munmap, MAP_FAILED, MAP_SHARED
#include <linux/fb.h>    // fb_var_screeninfo, fb_fix_screeninfo, FBIOGET_*
#include <stdlib.h>      // malloc, free
#include <string.h>      // memcpy, memset
#include <stdio.h>       // fprintf, stderr (仅 init 错误时用)


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
int lcd_init(lcd_display_t *lcd, const char *fb_dev, int src_w, int src_h)
{
    int fd = open(fb_dev, O_RDWR);
    if (fd < 0) {
        perror("open fb0");
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("FBIOGET_VSCREENINFO");
        close(fd);
        return -1;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("FBIOGET_FSCREENINFO");
        close(fd);
        return -1;
    }

    printf("LCD: %dx%d, %dbpp, line_len=%d\n",
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);

    size_t fb_size = finfo.line_length * vinfo.yres;
    unsigned char *fb = mmap(NULL, fb_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("mmap fb0");
        close(fd);
        return -1;
    }

    /* 填入结构体所有字段 */
    lcd->fd          = fd;
    lcd->fb          = fb;
    lcd->fb_size     = fb_size;
    lcd->xres        = vinfo.xres;
    lcd->yres        = vinfo.yres;
    lcd->bpp         = vinfo.bits_per_pixel;
    lcd->line_length = finfo.line_length;
    lcd->roff        = vinfo.red.offset   / 8;
    lcd->goff        = vinfo.green.offset / 8;
    lcd->boff        = vinfo.blue.offset  / 8;
    lcd->src_width   = src_w;
    lcd->src_height  = src_h;

    printf("RGB byte offsets: R=%d G=%d B=%d\n", lcd->roff, lcd->goff, lcd->boff);
    return 0;
}
int lcd_display_frame(lcd_display_t *lcd,frame_t *frame){
    unsigned char *rgb = malloc(lcd->src_width * lcd->src_height * 4);
    if(rgb == NULL){
        fprintf(stderr,"MALLOC RGB FAILED\n");
        return -1;
    }
    yuyv_to_rgb32(frame->data, rgb,
        lcd->src_width, lcd->src_height, lcd->roff, lcd->goff, lcd->boff);
    if(!frame || !frame->data){
        free(rgb);
        return -1;
    }
    unsigned char *dst = lcd->fb;
    unsigned char *src = rgb;
    for (int row = 0; row < lcd->src_height; row++) {
        memcpy(dst, src, lcd->src_width*4);
        dst += lcd->line_length;
        src += lcd->src_width*4;
    }
    free(rgb);
    return 0;
}
void lcd_clear(lcd_display_t *lcd){
    memset(lcd->fb,0,lcd->fb_size);
}
void lcd_destroy(lcd_display_t *lcd){
    munmap(lcd->fb,lcd->fb_size);
    close(lcd->fd);
}