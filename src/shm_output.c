#include "shm_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SHM_FRAME_NAME  "/fallwatch_frame"
#define SHM_EVENTS_NAME "/fallwatch_events"

int shm_output_init(shm_output_t *shm)
{
    if (!shm) return -1;

    memset(shm, 0, sizeof(*shm));
    shm->shm_fd_frame = -1;
    shm->shm_fd_events = -1;

    // 创建帧共享内存
    size_t frame_size = sizeof(struct shm_frame);
    shm->shm_fd_frame = shm_open(SHM_FRAME_NAME,
                                  O_CREAT | O_RDWR, 0666);
    if (shm->shm_fd_frame < 0) {
        perror("shm_open(frame)");
        return -1;
    }
    if (ftruncate(shm->shm_fd_frame, frame_size) < 0) {
        perror("ftruncate(frame)");
        close(shm->shm_fd_frame);
        shm->shm_fd_frame = -1;
        return -1;
    }
    shm->frame = mmap(NULL, frame_size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED, shm->shm_fd_frame, 0);
    if (shm->frame == MAP_FAILED) {
        perror("mmap(frame)");
        close(shm->shm_fd_frame);
        shm->shm_fd_frame = -1;
        return -1;
    }
    memset((void*)shm->frame, 0, frame_size);

    // 创建事件共享内存
    size_t events_size = sizeof(struct shm_events);
    shm->shm_fd_events = shm_open(SHM_EVENTS_NAME,
                                   O_CREAT | O_RDWR, 0666);
    if (shm->shm_fd_events < 0) {
        perror("shm_open(events)");
        munmap((void*)shm->frame, frame_size);
        close(shm->shm_fd_frame);
        shm->shm_fd_frame = -1;
        return -1;
    }
    if (ftruncate(shm->shm_fd_events, events_size) < 0) {
        perror("ftruncate(events)");
        close(shm->shm_fd_events);
        munmap((void*)shm->frame, frame_size);
        close(shm->shm_fd_frame);
        shm->shm_fd_frame = -1;
        return -1;
    }
    shm->events = mmap(NULL, events_size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED, shm->shm_fd_events, 0);
    if (shm->events == MAP_FAILED) {
        perror("mmap(events)");
        close(shm->shm_fd_events);
        munmap((void*)shm->frame, frame_size);
        close(shm->shm_fd_frame);
        shm->shm_fd_frame = -1;
        return -1;
    }
    memset((void*)shm->events, 0, events_size);

    printf("[shm_output] initialized\n");
    return 0;
}

void shm_output_write_frame(shm_output_t *shm,
                            const unsigned char *rgb,
                            int width, int height,
                            int frame_id)
{
    if (!shm || !shm->frame || !rgb) return;

    // 自旋锁：ready=1 告知 QT 正在写
    shm->frame->ready = 1;
    shm->frame->frame_id = frame_id;

    size_t row_len = (size_t)width * 3;
    size_t total = row_len * (size_t)height;
    if (total > sizeof(shm->frame->rgb))
        total = sizeof(shm->frame->rgb);

    memcpy((void*)shm->frame->rgb, rgb, total);

    // 解锁：ready=0 告知 QT 可以读
    shm->frame->ready = 0;
}

void shm_output_write_events(shm_output_t *shm,
                             int watcher, int motion, int warning)
{
    if (!shm || !shm->events) return;
    shm->events->watcher = watcher;
    shm->events->motion = motion;
    shm->events->warning = warning;
}

int shm_output_read_cancel(shm_output_t *shm)
{
    if (!shm || !shm->events) return 0;
    int val = shm->events->cancel;
    if (val) {
        shm->events->cancel = 0;  // 读取后清零
    }
    return val;
}

void shm_output_destroy(shm_output_t *shm)
{
    if (!shm) return;

    if (shm->events) {
        munmap((void*)shm->events, sizeof(struct shm_events));
        shm->events = NULL;
    }
    if (shm->shm_fd_events >= 0) {
        close(shm->shm_fd_events);
        shm_unlink(SHM_EVENTS_NAME);
        shm->shm_fd_events = -1;
    }
    if (shm->frame) {
        munmap((void*)shm->frame, sizeof(struct shm_frame));
        shm->frame = NULL;
    }
    if (shm->shm_fd_frame >= 0) {
        close(shm->shm_fd_frame);
        shm_unlink(SHM_FRAME_NAME);
        shm->shm_fd_frame = -1;
    }

    memset(shm, 0, sizeof(*shm));
    printf("[shm_output] destroyed\n");
}
