#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <pthread.h>
#include "ring_buffer.h"
#include "v4l2_capture.h"

typedef struct capture_thread_ctx{
  v4l2_capture_t *cap;      // 指向已初始化的采集设备
  ring_buffer_t  *rb;       // 指向已初始化的环形缓冲区
  pthread_t       thread;   // 线程 ID
  volatile int    running;  // 运行标志（1=跑，0=停）
  int             frame_id; // 帧计数器
} capture_thread_ctx;

void capture_thread_start(capture_thread_ctx *ctx);
void capture_thread_stop(capture_thread_ctx *ctx);

#endif