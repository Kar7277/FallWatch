#include "motion_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static frame_t *make_yuyv_frame(int w,int h,unsigned char y_val){
    frame_t *frame=(frame_t*)malloc(sizeof(frame_t));
    if(frame==NULL){
        fprintf(stderr,"make_yuyv_frame: malloc frame failed\n");
        return NULL;
    }
    frame->id=0;
    frame->timestamp=0;
    frame->data=(char*)malloc((size_t)w*h*2);
    if(frame->data==NULL){
        fprintf(stderr,"make_yuyv_frame: malloc frame data failed\n");
        free(frame);
        return NULL;
    }
    frame->data_size=w*h*2;
    for(int i=0;i<frame->data_size;i+=2){
        frame->data[i]=y_val;
        frame->data[i+1]=128;
    }
    return frame;
}
static void set_y_region(frame_t *f, int start, int count, unsigned char y_val){
    if(f == NULL) return;
    for(int i=start;i<start+count;i++){
        f->data[i*2] = y_val;
    }
}

#define CHECK(desc,cond) do{\
    total++;\
    if(cond){ printf("   PASS:%s\n",desc);passed++; }\
    else{ printf("   FAIL:%s\n",desc); }\
}while(0)

int main(void){
    int passed = 0, total = 0;
    
    {
    /* ══════════════════════════════════════════
     * Group 1: motion_detect_init 参数校验
     * ══════════════════════════════════════════ */
    printf("===== Group 1: motion_detect_init =====\n");
    motion_detect_t md;

    CHECK("init NULL md -> -1",
          motion_detect_init(NULL, 640, 480, 30, 0.1f) == -1);

    CHECK("init width=0 -> -1",
          motion_detect_init(&md, 0, 480, 30, 0.1f) == -1);

    CHECK("init height=0 -> -1",
          motion_detect_init(&md, 640, 0, 30, 0.1f) == -1);

    CHECK("init threshold=0 -> -1",
          motion_detect_init(&md, 640, 480, 0, 0.1f) == -1);

    CHECK("init threshold=256 -> -1",
          motion_detect_init(&md, 640, 480, 256, 0.1f) == -1);

    CHECK("init alarm_ratio=0 -> -1",
          motion_detect_init(&md, 640, 480, 30, 0.0f) == -1);

    CHECK("init alarm_ratio=1.5 -> -1",
          motion_detect_init(&md, 640, 480, 30, 1.5f) == -1);

    CHECK("init valid params -> 0",
          motion_detect_init(&md, 640, 480, 30, 0.1f) == 0);
    motion_detect_destroy(&md);
    }
    /* ══════════════════════════════════════════
     * Group 2: process 参数校验（4 项）
     * ══════════════════════════════════════════ */
    {
    motion_detect_t md2;
    motion_detect_init(&md2, 640, 480, 30, 0.1f);
    frame_t *valid = make_yuyv_frame(640, 480, 128);
    CHECK("process NULL md -> -1",
        motion_detect_process(NULL, valid) == -1);
    CHECK("process NULL frame -> -1",
        motion_detect_process(&md2, NULL) == -1);
    frame_t no_data = {0};
    no_data.data_size = 640*480*2;
    no_data.data = NULL;
    CHECK("process NULL frame->data -> -1",
        motion_detect_process(&md2, &no_data) == -1);
    frame_t small = {0};
    small.data_size = 640*480;
    small.data = malloc(640*480);
    CHECK("process data_size too small -> -1",
        motion_detect_process(&md2, &small) == -1);
    free(small.data);
    motion_detect_destroy(&md2);
    free(valid->data);
    free(valid);
    }
    /* ══════════════════════════════════════════
     * Group 3: 背景帧积累
     * ══════════════════════════════════════════ */
    {
    motion_detect_t md3;
    motion_detect_init(&md3, 640, 480, 30, 0.1f);
    frame_t *bg1 = make_yuyv_frame(640, 480, 100);
    frame_t *bg2 = make_yuyv_frame(640, 480, 110);
    CHECK("process bg_frame1 -> 0",
        motion_detect_process(&md3, bg1) == 0);
    CHECK("process bg_frame2 -> 0",
        motion_detect_process(&md3, bg2) == 0);
    motion_detect_destroy(&md3);
    free(bg1->data);
    free(bg1);
    free(bg2->data);
    free(bg2);
    }
    /* ══════════════════════════════════════════
     * Group 4: 三帧差法运动检测
     * ══════════════════════════════════════════ */

    {
    motion_detect_t md4;
    motion_detect_init(&md4, 640, 480, 30, 0.1f);
    frame_t *f1 = make_yuyv_frame(640, 480, 128);
    frame_t *f2 = make_yuyv_frame(640, 480, 128);
    frame_t *f3 = make_yuyv_frame(640, 480, 200);
    motion_detect_process(&md4, f1);
    motion_detect_process(&md4, f2);
    CHECK("process no motion -> 0",
        motion_detect_process(&md4, f3) == 0);
    motion_detect_destroy(&md4);
    free(f1->data);
    free(f1);
    free(f2->data);
    free(f2);
    free(f3->data);
    free(f3);
    }
    {
    /* ══════════════════════════════════════════
     * Group 4b :（AND=1 全像素检测）
     * ══════════════════════════════════════════ */
    motion_detect_t md5;
    motion_detect_init(&md5, 640, 480, 30, 0.1f);
    frame_t *f1 = make_yuyv_frame(640, 480, 100);
    frame_t *f2 = make_yuyv_frame(640, 480, 200);
    frame_t *f3 = make_yuyv_frame(640, 480, 100);
    motion_detect_process(&md5, f1);
    motion_detect_process(&md5, f2);
    CHECK("process have motion -> 1",
        motion_detect_process(&md5, f3) == 1);
    motion_detect_destroy(&md5);
    free(f1->data);
    free(f1);
    free(f2->data);
    free(f2);
    free(f3->data);
    free(f3);
    }
    /* ══════════════════════════════════════════
     * Group 4c ：阈值过滤
     * ══════════════════════════════════════════ */
    {
    motion_detect_t md6;
    motion_detect_init(&md6, 640, 480, 30, 0.1f);
    frame_t *f1 = make_yuyv_frame(640, 480, 128);
    frame_t *f2 = make_yuyv_frame(640, 480, 150);
    frame_t *f3 = make_yuyv_frame(640, 480, 128);
    motion_detect_process(&md6, f1);
    motion_detect_process(&md6, f2);
    CHECK("process no motion -> 0",
        motion_detect_process(&md6, f3) == 0);
    motion_detect_destroy(&md6);
    free(f1->data);
    free(f1);
    free(f2->data);
    free(f2);
    free(f3->data);
    free(f3);
    }
    /* ══════════════════════════════════════════
     * Group 4d ：比例过滤
     * ══════════════════════════════════════════ */
    {
    motion_detect_t md7;
    motion_detect_init(&md7, 640, 480, 30, 0.1f);
    frame_t *f1 = make_yuyv_frame(640, 480, 100);
    frame_t *f2 = make_yuyv_frame(640, 480, 200);
    frame_t *f3 = make_yuyv_frame(640, 480, 200);
    set_y_region(f3, 0, 50, 100); // 前半区域有运动，后半区域无运动
    motion_detect_process(&md7, f1);
    motion_detect_process(&md7, f2);
    CHECK("process no motion -> 0",
        motion_detect_process(&md7, f3) == 0);
    motion_detect_destroy(&md7);
    free(f1->data);
    free(f1);
    free(f2->data);
    free(f2);
    free(f3->data);
    free(f3);
    }
    /* ══════════════════════════════════════════
     * Group 5 ：destroy测试
     * ══════════════════════════════════════════ */
    {
    motion_detect_t md8;
    motion_detect_init(&md8, 640, 480, 30, 0.1f);
    motion_detect_destroy(&md8);
    CHECK("destroy after init (no crash)",1);
    printf("\n==============================\n");
    printf("  RESULT: %d/%d passed\n", passed, total);
    printf("==============================\n");
    return (passed == total) ? 0 : 1;
    }
}