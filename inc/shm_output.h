#ifndef SHM_OUTPUT_H
#define SHM_OUTPUT_H

#include <stddef.h>

// 共享内存中的帧数据 —— 和 QT LocalDataProvider 的 ShmFrame 布局一致
struct shm_frame {
    volatile int frame_id;
    volatile int ready;       // 1=C正在写, 0=QT可以读
    long long timestamp;
    unsigned char rgb[640 * 480 * 3];  // RGB24
};

// 共享内存中的事件数据
struct shm_events {
    volatile int watcher;     // 0=停, 1=运行
    volatile int motion;      // 0=无运动, 1=有运动
    volatile int warning;     // 0=正常, 1=报警中
    volatile int cancel;      // QT→C: 1=取消报警
};

typedef struct {
    int shm_fd_frame;
    int shm_fd_events;
    struct shm_frame *frame;
    struct shm_events *events;
} shm_output_t;

int  shm_output_init(shm_output_t *shm);
void shm_output_write_frame(shm_output_t *shm,
                            const unsigned char *rgb,
                            int width, int height,
                            int frame_id);
void shm_output_write_events(shm_output_t *shm,
                             int watcher, int motion, int warning);
int  shm_output_read_cancel(shm_output_t *shm);
void shm_output_destroy(shm_output_t *shm);

#endif // SHM_OUTPUT_H
