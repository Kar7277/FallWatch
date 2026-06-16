#ifndef JPEG_SAVE_H
#define JPEG_SAVE_H
#include <stdio.h>
#include <stddef.h>
#include "jpeglib.h"
#include <setjmp.h>
struct my_error_mgr{
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;  // setjmp/longjmp 跳转点
};

typedef struct jpeg_save_t{
    char output_dir[256];
    int quality;
    int width, height;
    struct jpeg_compress_struct cinfo;
    struct my_error_mgr jerr;
    int initialized;
}jpeg_save_t;

int jpeg_save_init(jpeg_save_t *js,const char *dir,int w,int h,int quality);
int jpeg_save_frame(jpeg_save_t *js,const void *yuyv_data,char *out_path,size_t path_len);
void jpeg_save_destroy(jpeg_save_t *js);
void yuyv_to_rgb_row(unsigned char* yuyv_data,unsigned char *rgb_row,int width);
#endif