#include "yuyv_to_rgb_neon.h"
#include <arm_neon.h>
void yuyv_to_rgb_row_neon(const unsigned char *yuyv_data,unsigned char *rgb_row,int width){
    const unsigned char *yuyv_ptr;
    unsigned char *rgb_ptr;
    for(int i = 0; i < width; i += 16) {
        // 每轮处理 16 像素 = 8 个宏像素
        yuyv_ptr = yuyv_data + i * 2;   // 16像素对应 32 字节 YUYV
        rgb_ptr  = rgb_row + i * 3;     // 16像素对应 48 字节 RGB

        // ① 加载 + 解交织（1条指令）
        uint8x8x4_t a = vld4_u8(yuyv_ptr);
        // ② 扩展 u8→u16 + 减偏移量
        uint16x8_t Y_even_u16 = vmovl_u8(a.val[0]);
        uint16x8_t U_u16 = vmovl_u8(a.val[1]);
        uint16x8_t Y_odd_u16 = vmovl_u8(a.val[2]);
        uint16x8_t V_u16 = vmovl_u8(a.val[3]);
        //减偏移量 Y -16  UV 都 - 128
        uint16x8_t C0 = vsubq_u16(Y_even_u16,vdupq_n_u16(16));
        uint16x8_t D = vsubq_u16(U_u16,vdupq_n_u16(128));
        uint16x8_t C1 = vsubq_u16(Y_odd_u16,vdupq_n_u16(16));
        uint16x8_t E = vsubq_u16(V_u16,vdupq_n_u16(128));
        // ③ 乘法累加（用 vmulq_n_u16 + vmlaq）
            //转成有符号
        int16x8_t s_C0 = vreinterpretq_s16_u16(C0);
        int16x8_t s_C1 = vreinterpretq_s16_u16(C1);
        int16x8_t s_D  = vreinterpretq_s16_u16(D);
        int16x8_t s_E  = vreinterpretq_s16_u16(E);
            //累加，转化成RGB
        //计算 R（偶 Y）：37×C0 + 51×E
        int16x8_t R0 = vmulq_n_s16(s_C0, 37);
        R0 = vmlaq_n_s16(R0, s_E, 51);     // R0 = 37×C0 + 51×E
            //计算 G（偶 Y）：37×C0 - 100×D - 208×E
        int16x8_t G0 = vmulq_n_s16(s_C0, 37);
        G0 = vmlaq_n_s16(G0, s_D, -13);    // G0 = 37×C0 - 100×D
        G0 = vmlaq_n_s16(G0, s_E, -26);    // G0 = 37×C0 - 100×D - 208×E
            //计算 B（偶 Y）：37×C0 + 65×D
        int16x8_t B0 = vmulq_n_s16(s_C0, 37);
        B0 = vmlaq_n_s16(B0, s_D, 65);     // B0 = 37×C0 + 65×D
        //计算 奇Y的R1 G1 B1
        int16x8_t R1 = vmulq_n_s16(s_C1, 37);
        R1 = vmlaq_n_s16(R1, s_E, 51);     // R0 = 37×C0 + 51×E
            //计算 G（奇Y）：37×C0 - 100×D - 208×E
        int16x8_t G1 = vmulq_n_s16(s_C1, 37);
        G1 = vmlaq_n_s16(G1, s_D, -13);    // G0 = 37×C0 - 100×D
        G1 = vmlaq_n_s16(G1, s_E, -26);    // G0 = 37×C0 - 100×D - 208×E
            //计算 B（奇Y）：37×C0 + 65×D
        int16x8_t B1 = vmulq_n_s16(s_C1, 37);
        B1 = vmlaq_n_s16(B1, s_D, 65);     // B0 = 37×C0 + 65×D
        // ④ 移位窄化回 u8（vqrshrn，自带饱和）
        uint8x8_t r0_u8 = vqrshrun_n_s16(R0, 5);   // 偶 Y 的 R
        uint8x8_t g0_u8 = vqrshrun_n_s16(G0, 5);   // 偶 Y 的 G
        uint8x8_t b0_u8 = vqrshrun_n_s16(B0, 5);   // 偶 Y 的 B
        uint8x8_t r1_u8 = vqrshrun_n_s16(R1, 5);   // 奇 Y 的 R
        uint8x8_t g1_u8 = vqrshrun_n_s16(G1, 5);   // 奇 Y 的 G
        uint8x8_t b1_u8 = vqrshrun_n_s16(B1, 5);   // 奇 Y 的 B
        // ⑤ 交织存出（1条指令）
        uint8x8x2_t r_pair = vzip_u8(r0_u8, r1_u8);
        uint8x8x2_t g_pair = vzip_u8(g0_u8, g1_u8);
        uint8x8x2_t b_pair = vzip_u8(b0_u8, b1_u8);
        uint8x8x3_t rgb0 = {{ r_pair.val[0], g_pair.val[0], b_pair.val[0] }};
        uint8x8x3_t rgb1 = {{ r_pair.val[1], g_pair.val[1], b_pair.val[1] }};
        vst3_u8(rgb_ptr, rgb0);         // 前 8 像素 → 24 字节
        vst3_u8(rgb_ptr + 24, rgb1);    // 后 8 像素 → 24 字节
    }
}

