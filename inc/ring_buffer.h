#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include <pthread.h>
/* 一帧图像数据（元数据 + 堆上数据指针，避免栈上大数组） */
typedef struct frame_t {
    int id;                     // 帧唯一标识符
    unsigned char *data;                 // 指向堆上帧数据，大小由 data_size 决定
    int data_size;              // 帧数据实际字节数（如 640*480=307200）
    long long timestamp;        // 时间戳(ms)，用于帧率计算和丢帧检测
} frame_t;
/* 环形缓冲区 */
typedef struct ring_buffer_t {
    int capacity;               // 槽位数量
    frame_t **buffer;           // 帧指针数组
    int head;                   // 生产者写入位置
    int tail;                   // 消费者读取位置
    int count;                  // 当前缓冲帧数
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    volatile int active;
} ring_buffer_t;
ring_buffer_t *ring_buffer_init(int capacity);
void           ring_buffer_destroy(ring_buffer_t *rb);
/* 存入一帧。调用后 frame 所有权转移给缓冲区，调用方不得再读写或释放该帧 */
void           ring_buffer_put(ring_buffer_t *rb, frame_t *frame);
/* 取出一帧。调用方获得所有权，用完后必须 frame_free 释放 */
frame_t       *ring_buffer_get(ring_buffer_t *rb);
/* 安全释放一帧（先 free data，再 free 结构体） */
void           frame_free(frame_t *frame);
/*非阻塞取帧，空时返回NULL*/
frame_t *ring_buffer_get_nonblock(ring_buffer_t *rb);
int ring_buffer_stop(ring_buffer_t *rb);
int ring_buffer_count(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */