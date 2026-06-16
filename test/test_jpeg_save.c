#include "jpeg_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(void){
    jpeg_save_t *js = (jpeg_save_t *)malloc(sizeof(jpeg_save_t));
    //非法参数测试：
    char dir[64];
    int w = 0;
    int h = 0;
    int quality = 1111;
    if(jpeg_save_init(js,NULL,w,h,quality) != 0){
        printf("TEST_INVALID_VALUE SUCCESS");
    }
    strncpy(dir,"/tmp",sizeof(dir));
    dir[sizeof(dir) - 1] = '\0';
    w = 640;
    h = 480;
    quality = 80;
    if(jpeg_save_init(js,dir,w,h,quality) == 0){
        printf("TEST_VALID_VALUE SUCCESS\n");
    }
    char out_path[512] = "";
    unsigned char *yuyv_data = (unsigned char *)malloc(640*480*2);
    if(yuyv_data == NULL){
        printf("YUYV_DATA INIT FAILED\n");
        goto cleanup;
    }
    for(int i = 0;i < w * h/2;i++){
        yuyv_data[i*4 + 0] =  128;
        yuyv_data[i*4 + 1] =  128;
        yuyv_data[i*4 + 2] =  128;
        yuyv_data[i*4 + 3] =  128;
    }
    size_t path_len = sizeof(out_path);
    
    if(jpeg_save_frame(js,yuyv_data,out_path,path_len) == 0){
        FILE *fp = fopen(out_path,"rb");
        if(fp == NULL){
            printf("OPEN JPEG_FILE FAILED\n");
            goto cleanup;
        }
        fseek(fp,0,SEEK_END);
        if(ftell(fp) > 0){
            printf("TEST_SAVE_FRAME SUCCESS\n");
        }
        else{
            printf("TEST_SAVE_FRAME FAILED\n");
        }
        fclose(fp);
    }
    cleanup:
    free(yuyv_data);
    jpeg_save_destroy(js);
    jpeg_save_destroy(js);//双重销毁
    free(js);
    return 0;
}