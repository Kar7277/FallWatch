#include "jpeg_save.h"
#include "yuyv_to_rgb_neon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
static void my_error_exit(j_common_ptr cinfo){
    struct my_error_mgr *myerr = (struct my_error_mgr *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer,1);
}
#define CLAMP(v) ((v) < 0 ? 0 : (v) > 255 ? 255 : (v)) 
void yuyv_to_rgb_row(unsigned char* yuyv_data,unsigned char *rgb_row,int width){
    for(int i = 0;i < width/2;i++){
        int Y0 = yuyv_data[i*4 + 0];
        int U  = yuyv_data[i*4 + 1];
        int Y1 = yuyv_data[i*4 + 2];
        int V  = yuyv_data[i*4 + 3];

        int C0 = Y0 - 16;
        int C1 = Y1 - 16;
        int D  = U  - 128;
        int E  = V  - 128;

        rgb_row[i*6 + 0] = CLAMP((298 * C0 + 409 * E + 128) >> 8);
        rgb_row[i*6 + 1] = CLAMP((298 * C0 - 100 * D - 208 * E + 128) >> 8);
        rgb_row[i*6 + 2] = CLAMP((298 * C0 + 516 * D + 128) >> 8);

        rgb_row[i*6 + 3] = CLAMP((298 * C1 + 409 * E + 128) >> 8);
        rgb_row[i*6 + 4] = CLAMP((298 * C1 - 100 * D - 208 * E + 128) >> 8);
        rgb_row[i*6 + 5] = CLAMP((298 * C1 + 516 * D + 128) >> 8);
    }
}
int jpeg_save_init(jpeg_save_t *js,const char *dir,int w,int h,int quality){
    if(js == NULL){
        fprintf(stderr,"JS IS NULL INIT FAILED\n");
        return -1;
    }
    js->initialized = 0;
    if(dir == NULL || w <= 0  || h <= 0 || quality < 1 || quality > 100){
        fprintf(stderr,"OTHER VALUE IS INVALID INIT FAILED\n");
        return -1;
    }
    strncpy(js->output_dir,dir,sizeof(js->output_dir));
    mkdir(dir, 0755);
    js->output_dir[sizeof(js->output_dir) - 1] = '\0';
    js->width = w;
    js->height = h;
    js->quality = quality;
    js->cinfo.err = jpeg_std_error(&js->jerr.pub);
    js->jerr.pub.error_exit = my_error_exit;
    jpeg_create_compress(&js->cinfo);
    js->initialized = 1;
    return 0;
}
int jpeg_save_frame(jpeg_save_t *js,const void *yuyv_data,char *out_path,size_t path_len){
    //文件命名
    char name[64];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(name,sizeof(name),"snap_%Y%m%d_%H%M%S.jpg",tm);
    snprintf(out_path,path_len,"%s/%s",js->output_dir,name);
    FILE *fp = fopen(out_path,"wb");
    if(fp == NULL){
        fprintf(stderr,"OPEN JPEG_FP FAILED\n");
        return -1;
    }
    unsigned char *rgb_row = malloc(js->width * 3);
    JSAMPROW row_pointer[1];
    if(setjmp(js->jerr.setjmp_buffer)){
        jpeg_destroy_compress(&js->cinfo);
        js->initialized = 0;
        fclose(fp);
        remove(out_path);
        free(rgb_row);
        return -1;
    }
//cinfo 设值
    js->cinfo.image_width = js->width;
    js->cinfo.image_height = js->height;
    js->cinfo.input_components = 3;
    js->cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&js->cinfo);
    jpeg_set_quality(&js->cinfo,js->quality,TRUE);
    jpeg_stdio_dest(&js->cinfo,fp);
//
    jpeg_start_compress(&js->cinfo,TRUE);

    for(int row = 0; row < js->height; row++){
        // ① YUYV → RGB 转换
        #ifdef __ARM_NEON__
            yuyv_to_rgb_row_neon((unsigned char*)yuyv_data + row * js->width * 2,rgb_row,js->width);
        #else
            yuyv_to_rgb_row((unsigned char*)yuyv_data + row * js->width * 2,rgb_row,js->width);
        #endif
    // ② 把行指针指向转换好的 RGB 数据
    row_pointer[0] = rgb_row;
    // ③ 喂给 libjpeg
    jpeg_write_scanlines(&js->cinfo,row_pointer,1);
    }
    jpeg_finish_compress(&js->cinfo);
    fclose(fp);
    free(rgb_row);
    return 0;
}
void jpeg_save_destroy(jpeg_save_t *js){
    if(js == NULL){
        fprintf(stderr,"JS IS NULL DESTROY FAILED\n");
        return;
    }
    if(js->initialized == 1){
        jpeg_destroy_compress(&js->cinfo);
        js->initialized = 0;
    }
    
}