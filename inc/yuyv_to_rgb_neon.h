#ifndef YUYV_TO_RGB_NEON_H
#define YUYV_TO_RGB_NEON_H
void yuyv_to_rgb_row_neon(const unsigned char *yuyv_data,unsigned char *rgb_row,int width);
#endif