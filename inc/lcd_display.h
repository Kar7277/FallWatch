#ifndef LCD_DISPLAY
#define LCD_DISPLAY
#include "ring_buffer.h"
typedef struct lcd_display_t{
    int fd;           //设备的文件描述符    
    unsigned char *fb;//指向显存的指针
    size_t fb_size;   //现存所需字节数
    int xres,yres;//LCD的物理宽高度
    int bpp;//每像素位数：bit per pixel
    int line_length;//一行占多少字节
    int roff,goff,boff;//RGB分量在32位像素里的字节偏移
    int src_width,src_height;//摄像头帧的宽高度
}lcd_display_t;

int lcd_init(lcd_display_t  *lcd,const char *fb_dev,int src_w,int src_h);
int lcd_display_frame(lcd_display_t *lcd,frame_t *frame);
void lcd_clear(lcd_display_t *lcd);
void lcd_destroy(lcd_display_t *lcd);


#endif