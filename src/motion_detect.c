#include "motion_detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void extract_y(const unsigned char *yuyv_data, unsigned char *dst, int total_pixels){
    for(int i=0;i<total_pixels;i++){
        dst[i]=yuyv_data[i*2];
    }
}//提取Y分量

int motion_detect_init(motion_detect_t *md,int width,int height,int threshold,float alarm_ratio){
    if(md==NULL){
        fprintf(stderr,"motion_detect_init: md is NULL\n");
        return -1;
    }
    if (width<=0 || height<=0 || (threshold<=0 || threshold>255) || alarm_ratio<=0 || alarm_ratio>1){
        fprintf(stderr,"motion_detect_init: invalid parameters\n");
        return -1;
    }
    md->width=width;
    md->height=height;
    md->threshold=threshold;
    md->alarm_ratio=alarm_ratio;
    md->bg_frame_ready=0;
    int total_pixels=width*height;
    md->y_buffer_curr=(unsigned char*)malloc((size_t)total_pixels * sizeof(unsigned char));
    md->y_buffer_prev1=(unsigned char*)malloc((size_t)total_pixels * sizeof(unsigned char));
    md->y_buffer_prev2=(unsigned char*)malloc((size_t)total_pixels * sizeof(unsigned char));
    if(md->y_buffer_curr==NULL || md->y_buffer_prev1==NULL || md->y_buffer_prev2==NULL){
        fprintf(stderr,"motion_detect_init: malloc y_buffer_ failed\n");
        if(md->y_buffer_curr){
            free(md->y_buffer_curr);
            md->y_buffer_curr=NULL;
        }
        if(md->y_buffer_prev1){
            free(md->y_buffer_prev1);
            md->y_buffer_prev1=NULL;
        }
        if(md->y_buffer_prev2){
            free(md->y_buffer_prev2);
            md->y_buffer_prev2=NULL;
        }
        return -1;
    }
    return 0;
}
int motion_detect_process(motion_detect_t *md,frame_t *frame){
    if(md==NULL || frame==NULL || frame->data==NULL){
        fprintf(stderr,"motion_detect_process: invalid parameters\n");
        return -1;
    }
    int total_pixels=md->width*md->height;
    int motion_pixels=0;
    if(frame->data_size < total_pixels*2){
        fprintf(stderr,"motion_detect_process: frame data size too small\n");
        return -1;
    }
    extract_y((const unsigned char*)frame->data,md->y_buffer_curr,total_pixels);//提取Y分量到当前缓冲区
    if(md->bg_frame_ready < 2){
        // 指针轮转：prev2 ←prev1 ←curr ←prev2（三指针循环右移）
        unsigned char *tmp = md->y_buffer_prev2;
        md->y_buffer_prev2 = md->y_buffer_prev1;
        md->y_buffer_prev1 = md->y_buffer_curr;
        md->y_buffer_curr = tmp;
        md->bg_frame_ready++;
        return 0;//前两帧作为背景，暂不检测
    }
    for(int i=0;i<total_pixels;i++){
        int diff1=abs(md->y_buffer_prev2[i]-md->y_buffer_prev1[i]);
        int diff2=abs(md->y_buffer_prev1[i]-md->y_buffer_curr[i]);
        if(diff1>md->threshold && diff2>md->threshold){
            motion_pixels++;
        }
    }
    //指针轮转
    unsigned char *tmp = md->y_buffer_prev2;
    md->y_buffer_prev2 = md->y_buffer_prev1;
    md->y_buffer_prev1 = md->y_buffer_curr;
    md->y_buffer_curr = tmp;
    if((float)motion_pixels/total_pixels > md->alarm_ratio){
        return 1;//报警
    }
    return 0;//正常
}
void motion_detect_destroy(motion_detect_t *md){
    if(md == NULL){
        fprintf(stderr,"motion_detect_destroy: md is NULL\n");
    }
    if(md->y_buffer_curr){
        free(md->y_buffer_curr);
        md->y_buffer_curr=NULL;
    }
    if(md->y_buffer_prev1){
        free(md->y_buffer_prev1);
        md->y_buffer_prev1=NULL;
    }
    if(md->y_buffer_prev2){
        free(md->y_buffer_prev2);
        md->y_buffer_prev2=NULL;
    }
}