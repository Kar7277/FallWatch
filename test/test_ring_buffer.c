#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 测试辅助函数 ── */

/* 创建一帧，用指定字节填充数据（模拟真实图像数据） */
frame_t *make_test_frame(int id, int data_size) {
    frame_t *f = (frame_t *)malloc(sizeof(frame_t));
    if (!f) return NULL;
    f->id = id;
    f->data_size = data_size;
    f->data = (char *)malloc(data_size);
    if (!f->data) {
        free(f);
        return NULL;
    }
    memset(f->data, 0xAA, data_size);
    f->timestamp = 1000 + id * 33;
    return f;
}

/* 验证帧数据是否完整 */
int verify_frame(frame_t *f, int expected_id, int expected_size) {
    if (f->id != expected_id) {
        printf("  FAIL: id 不对, 期望 %d, 实际 %d\n", expected_id, f->id);
        return 0;
    }
    if (f->data_size != expected_size) {
        printf("  FAIL: data_size 不对\n");
        return 0;
    }
    for (int i = 0; i < expected_size; i++) {
        if (f->data[i] != (char)0xAA) {
            printf("  FAIL: data[%d] 被篡改\n", i);
            return 0;
        }
    }
    printf("  PASS: id=%d, size=%d, data 完整\n", f->id, f->data_size);
    return 1;
}

/* ── 主测试 ── */
int main() {
    int passed = 0, total = 0;

    ring_buffer_t *rb = ring_buffer_init(5);

    /* 测试1：基本放入/取出 */
    printf("【测试1】放入3帧, 取出3帧\n");
    for (int i = 0; i < 3; i++) {
        ring_buffer_put(rb, make_test_frame(i, 256));
    }
    for (int i = 0; i < 3; i++) {
        total++;
        frame_t *f = ring_buffer_get(rb);
        if (f && verify_frame(f, i, 256)) passed++;
        frame_free(f);
    }

    /* 测试2：空缓冲区应返回 NULL */
    printf("【测试2】空缓冲区取帧\n");
    total++;
    if (ring_buffer_get(rb) == NULL) {
        printf("  PASS: 正确返回 NULL\n");
        passed++;
    }

    /* 测试3：满覆盖 */
    printf("【测试3】满覆盖: 塞7帧到容量5的缓冲区\n");
    for (int i = 0; i < 7; i++) {
        ring_buffer_put(rb, make_test_frame(i, 128));
    }
    for (int i = 2; i < 7; i++) {
        total++;
        frame_t *f = ring_buffer_get(rb);
        if (f && verify_frame(f, i, 128)) passed++;
        frame_free(f);
    }
    total++;
    if (ring_buffer_get(rb) == NULL) passed++;

    ring_buffer_destroy(rb);

    printf("\n═══════════════\n");
    printf("结果: %d/%d 通过\n", passed, total);
    getchar();
    return (passed == total) ? 0 : 1;

}
