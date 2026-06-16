#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void frame_free(frame_t *frame) {
    if (frame) {
        free(frame->data); // 释放帧数据
        free(frame);       // 释放帧结构体
    }
}
ring_buffer_t *ring_buffer_init(int capacity) {
    ring_buffer_t *rb = (ring_buffer_t *)malloc(sizeof(ring_buffer_t));
    if (!rb) return NULL;
    if(pthread_mutex_init(&rb->mutex,NULL) != 0){
        fprintf(stderr,"Mutex Initial Failed\n");
        pthread_mutex_destroy(&rb->mutex);
        free(rb);
        return NULL;
    }
    if(pthread_cond_init(&rb->not_empty,NULL) != 0){
        fprintf(stderr,"not_empty Initial Failed\n");
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->mutex);
        free(rb);
        return NULL;
    }
    if(pthread_cond_init(&rb->not_full,NULL) != 0){
        fprintf(stderr,"not_full Initial Failed\n");
        pthread_cond_destroy(&rb->not_full);
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->mutex);
        free(rb);
        return NULL;
    }
    rb->active = 1;
    rb->capacity = capacity;
    rb->buffer = (frame_t **)malloc(sizeof(frame_t *) * capacity);
    if (!rb->buffer) {
        pthread_cond_destroy(&rb->not_full);
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->mutex);
        free(rb);
        return NULL;
    }
    memset(rb->buffer, 0, sizeof(frame_t *) * capacity);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return rb;
}

void ring_buffer_destroy(ring_buffer_t *rb) {
    if (!rb) return;
    ring_buffer_stop(rb);
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
    if(rb->buffer){
        for (int i = 0; i < rb->capacity; i++) {
        if (rb->buffer[i]) {
            frame_free(rb->buffer[i]);
            }
        }
    }
    free(rb->buffer);
    free(rb);
}

void ring_buffer_put(ring_buffer_t *rb, frame_t *frame) {
    if (!rb || !frame) return;
    pthread_mutex_lock(&rb->mutex);
    while(rb->count == rb->capacity && rb->active){
        pthread_cond_wait(&rb->not_full,&rb->mutex);
    }
    if(!rb->active){
        pthread_mutex_unlock(&rb->mutex);
        return;
    }
    rb->buffer[rb->head] = frame;
    rb->head = (rb->head+1) % rb->capacity;
    rb->count++;
    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->mutex);
}


frame_t *ring_buffer_get(ring_buffer_t *rb) {
    if (!rb) return NULL; // 没有帧可读
    pthread_mutex_lock(&rb->mutex);
    while (rb->count == 0 && rb->active) {                  // 空了就等，但 active=0 时不再等
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }
    if (!rb->active && rb->count == 0) {
        pthread_mutex_unlock(&rb->mutex);
        return NULL; }  // 停止 + 空了 → 返回 NULL
    frame_t *frame = rb->buffer[rb->tail];
    rb->tail = (rb->tail+1) % rb->capacity;
    rb->count--;
    pthread_cond_signal(&rb->not_full);                       // "有空位了，醒醒"
    pthread_mutex_unlock(&rb->mutex);
    return frame;
}

frame_t *ring_buffer_get_nonblock(ring_buffer_t *rb){
    if (!rb) return NULL;
    pthread_mutex_lock(&rb->mutex);
    if (rb->count == 0) {
        pthread_mutex_unlock(&rb->mutex); return NULL;
    }         // 和 get 的区别：不等，直接返回 NULL
    frame_t *frame = rb->buffer[rb->tail];//取出 frame，tail++，count--
    rb->tail = (rb->tail+1) % rb->capacity;
    rb->count--;
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    return frame;
}
int ring_buffer_stop(ring_buffer_t *rb){
    pthread_mutex_lock(&rb->mutex);
    rb->active = 0;
    pthread_cond_broadcast(&rb->not_empty);                    // 广播：唤醒所有在等 not_empty 的消费者
    pthread_cond_broadcast(&rb->not_full);                        // 广播：唤醒所有在等 not_full 的生产者
    pthread_mutex_unlock(&rb->mutex);
    return 0;
}
int ring_buffer_count(ring_buffer_t *rb){
    pthread_mutex_lock(&rb->mutex);
    int count = rb->count;
    pthread_mutex_unlock(&rb->mutex);
    return count;
}
