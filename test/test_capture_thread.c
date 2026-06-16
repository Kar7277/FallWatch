#include "capture_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
static int verify_frame(frame_t *f){
    if (f->data == NULL) {
        fprintf(stderr, "verify_frame: data pointer is NULL\n");
        return 0;
    }
    if (f->data_size != 640*480*2) {
        fprintf(stderr, "verify_frame: invalid data_size %d\n", f->data_size);
        return 0;
    }
    if (f->id < 0) {
        fprintf(stderr, "verify_frame: invalid frame id %d\n", f->id);
        return 0;
    }
    return 1;
}
int main(void){
    capture_thread_ctx ctx = {0};
    ring_buffer_t *rb = ring_buffer_init(8);
    v4l2_capture_t cap = {0};
    ctx.cap = &cap;
    if (!rb) {
        fprintf(stderr, "Failed to init ring buffer\n");
        return EXIT_FAILURE;
    }
    ctx.rb = rb;
    if (v4l2_init(ctx.cap, "/dev/video1", 640, 480, rb) != 0) {
        fprintf(stderr, "Failed to init v4l2 capture\n");
        ring_buffer_destroy(rb);
        return EXIT_FAILURE;
    }
    if (v4l2_start(ctx.cap) != 0) {
        fprintf(stderr, "Failed to start v4l2 capture\n");
        v4l2_destroy(ctx.cap);
        ring_buffer_destroy(rb);
        return EXIT_FAILURE;
    }
    capture_thread_start(&ctx);
    int consumed = 0, verified = 0;
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < 10) { // 运行10秒
        frame_t *f = ring_buffer_get(rb);
        if (f) {
            consumed++;
            if (verify_frame(f)) verified++;
            frame_free(f);
        }else {
            // 没有帧可读，可能是生产者还在采集或缓冲区暂时空了
            usleep(50000); // 每50ms尝试取帧一次
        }
    }
    printf("[run] consumed: %d, verified: %d\n", consumed, verified);
    capture_thread_stop(&ctx);
    frame_t *f;
    int consumed_drain = 0, verified_drain = 0;
    while ((f = ring_buffer_get(rb)) != NULL) {
        consumed_drain++;
        if (verify_frame(f)) verified_drain++;
        frame_free(f);
    }
    
    printf("[drain] consumed: %d, verified: %d\n", consumed_drain, verified_drain);
    //检查缓冲区是否有帧未被消费，正常情况下应该没有
    printf("[TOTAL] consumed: %d, verified: %d\n", consumed + consumed_drain, verified + verified_drain);
    v4l2_destroy(ctx.cap);
    ring_buffer_destroy(rb);
    if (consumed + consumed_drain == 0) {
        printf("RESULT: FAIL\n");
        return EXIT_FAILURE;
    } else {
        printf("RESULT: PASS\n");
        return EXIT_SUCCESS;
    }
}