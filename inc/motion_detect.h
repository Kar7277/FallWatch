#ifndef MOTION_DETECT_H
#define MOTION_DETECT_H
#include "ring_buffer.h"
typedef struct motion_detect_t {
    unsigned char *y_buffer_curr;
    unsigned char *y_buffer_prev1;
    unsigned char *y_buffer_prev2;
    int width; //帧宽度
    int height; //帧高度
    int threshold; //运动检测阈值
    float alarm_ratio; //报警比例
    int bg_frame_ready; //背景帧准备就绪标志
}motion_detect_t;
int motion_detect_init(motion_detect_t *md,int width,int height,int threshold,float alarm_ratio);
int motion_detect_process(motion_detect_t *md,frame_t *frame);
void motion_detect_destroy(motion_detect_t *md);
#endif // MOTION_DETECT_H