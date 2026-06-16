#include "fallwatch.h"
#include <signal.h>
#include <stdlib.h>
volatile int g_running = 1;// 全局变量（信号处理器用
static ring_buffer_t *g_rb = NULL;
static void sig_handler(int sig){
    (void)sig;
    g_running = 0;
    if(g_rb) ring_buffer_stop(g_rb);// 设置停止标志
}

int main(void){
//-------初始化各模块----------
    signal(SIGINT,sig_handler);
    signal(SIGTERM,sig_handler);
    fallwatch_config_t cfg;
    config_load("config.ini", &cfg);
    int total = 0;
    int pass = 0;
    int ret = 0;
    ring_buffer_t *rb = ring_buffer_init(8); // 8个缓冲区
    g_rb = rb;
    v4l2_capture_t cap = {0};
    capture_thread_ctx ctx = {0};
    ctx.cap = &cap;
    ctx.rb = rb;
    motion_detect_t md = {0};
    gpio_alert_t gpio = {0};
    mqtt_publisher_t mq = {0};
    lcd_display_t lcd = {0};
    jpeg_save_t js = {0};
    shm_output_t shm = {0};
    sms_alert_t *sms = sms_alert_init(cfg.sms.uart_dev,cfg.sms.phone);
    
    if(rb == NULL){
        fprintf(stderr,"RING_BUFFER IS NULL\n");
        goto cleanup;
    }
    if(v4l2_init(&cap, cfg.camera.device_path, cfg.camera.width, cfg.camera.height, rb)){
        fprintf(stderr,"V4L2_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    printf("V4L2_INIT SUCCESS\n");
    total++;
    pass++;
    if(motion_detect_init(&md,cfg.camera.width,cfg.camera.height,cfg.motion.threshold,cfg.motion.alarm_ratio)){
        fprintf(stderr,"MOTION_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    printf("MOTION_INIT SUCCESS\n");
    total++;
    pass++;
    if(gpio_alert_init(&gpio,cfg.gpio.led_pin)){
        fprintf(stderr,"GPIO_ALERT_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    gpio_led_off(&gpio);
    printf("GPIO_ALERT_INIT SUCCESS\n");
    total++;
    pass++;
    if(sms == NULL){
        fprintf(stderr,"SMS_ALERT_INIT FAILED\n");
        total++;
    }
    else{
        printf("SMS_ALERT_INIT SUCCESS\n");
        pass++;
        total++;
    }
    int mq_flag = 1;
    if(mqtt_init(&mq,cfg.mqtt.broker_uri,cfg.mqtt.topic,cfg.mqtt.client_id)){
        fprintf(stderr,"MQTT_INIT FAILED\n");
        mq_flag = 0;
        total++;
    }
    else{
        printf("MQTT_INIT SUCCESS\n");
        pass++;
        total++;
    }
    if(lcd_init(&lcd,cfg.lcd.fb_dev,cfg.camera.width,cfg.camera.height)){
        fprintf(stderr,"LCD_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    printf("LCD_INIT SUCCESS\n");
    total++;
    pass++;
    if(jpeg_save_init(&js,cfg.jpeg.save_dir,cfg.camera.width,cfg.camera.height,cfg.jpeg.quality)){
        fprintf(stderr,"JPEG_SAVE_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    printf("JPEG_SAVE_INIT SUCCESS\n");
    total++;
    pass++;
    if(shm_output_init(&shm)){
        fprintf(stderr,"SHM_OUTPUT_INIT FAILED\n");
        ret = -1;
        total++;
        goto cleanup;
    }
    printf("SHM_OUTPUT_INIT SUCCESS\n");
    total++;
    pass++;
    printf("%d/%d INIT MODULE SUCCESS\n",pass,total);
    v4l2_start(&cap);
    capture_thread_start(&ctx);
    printf("=== ENTERING MAIN LOOP ===\n");
    unsigned char *rgb = NULL;
//-------主线程循环----------
    rgb = malloc(cfg.camera.width * cfg.camera.height * 3);
    if(!rgb){
        fprintf(stderr,"MALLOC_RGB FAILED\n");
        ret = -1;
        goto cleanup;
    }
    frame_t *frame = {0};
    int led_is_on = 0;
while(g_running){                              // ← 外层：运动检测模式
    frame = ring_buffer_get(rb);
    if(!frame) goto cleanup;

    //yuyv转化成RGB
    for(int i=0;i<cfg.camera.height;i++){
        #ifdef __ARM_NEON__
            yuyv_to_rgb_row_neon(frame->data + i * cfg.camera.width * 2,rgb + i * cfg.camera.width * 3,cfg.camera.width);
        #else
            yuyv_to_rgb_row(frame->data + i * cfg.camera.width * 2,rgb + i * cfg.camera.width * 3,cfg.camera.width);
        #endif    
    }
    shm_output_write_frame(&shm,rgb,cfg.camera.width,cfg.camera.height,frame->id);// 每帧预览不变

    if(motion_detect_process(&md, frame)){
        // 上升沿：SMS + MQTT 各发一次
        shm_output_write_events(&shm,1,1,1);  // warning=1 
        if(sms != NULL){
            if(sms_alert_send(sms,"THE ELDERLY FALL DOWN!!!\n")){
                fprintf(stderr,"SMS_ALERT_SEND FAILED\n");
                ret = -1;
                frame_free(frame);
                goto cleanup;
            }
            printf("SMS_ALERT_SEND SUCCESS\n");
        }
        //发 MQTT
        if(mq_flag != 0){
             if(mqtt_publish_alert(&mq,"THE ELDERLY FALL DOWN!!!\n")){
                fprintf(stderr,"MQTT_PUBLISH_ALERT FAILED\n");
                ret = -1;
                frame_free(frame);
                goto cleanup;
            }
        printf("MQTT_PUBLISH_ALERT SUCCESS\n");
        }
        char jpeg_path[512]; 
            if(jpeg_save_frame(&js,frame->data,jpeg_path,sizeof(js.output_dir))){
                fprintf(stderr,"JPEG_SAVE_FRAME FAILED\n");
                ret = -1;
                frame_free(frame);
                goto cleanup;
            }             // 存图

            printf("JPEG_SAVE_FRAME SUCCESS\n");
        frame_free(frame);                     // 释放当前帧
        while(g_running){                      // ← 内层：报警模式
            frame = ring_buffer_get(rb);
            if(!frame) goto cleanup;
            for(int i=0;i<cfg.camera.height;i++){
                #ifdef __ARM_NEON__
                    yuyv_to_rgb_row_neon(frame->data + i * cfg.camera.width * 2,rgb + i * cfg.camera.width * 3,cfg.camera.width);
                #else
                    yuyv_to_rgb_row(frame->data + i * cfg.camera.width * 2,rgb + i * cfg.camera.width * 3,cfg.camera.width);
                #endif
            }
            shm_output_write_frame(&shm,rgb,cfg.camera.width,cfg.camera.height,frame->id);// 每帧预览不变
            if(!led_is_on){
                gpio_led_on(&gpio);
                led_is_on = 1;
            }                // LED 常亮
            if(shm_output_read_cancel(&shm)){  // 只等 cancel
                if(led_is_on){
                    gpio_led_off(&gpio);
                    led_is_on = 0;
                }
                shm_output_write_events(&shm,1,0,0);
                frame_free(frame);
                break;                         // 退出报警循环
            }
            frame_free(frame);
        }
        continue;                              // 回到外层运动检测
    }
    frame_free(frame);
}
//-------清理-------
    cleanup:
    free(rgb);
    capture_thread_stop(&ctx);
    ring_buffer_stop(rb);
    ring_buffer_destroy(rb);
    v4l2_destroy(&cap);
    motion_detect_destroy(&md);
    sms_alert_destroy(sms);
    gpio_led_off(&gpio);
    gpio_alert_destroy(&gpio);
    mqtt_destroy(&mq);
    lcd_clear(&lcd);
    lcd_destroy(&lcd);
    jpeg_save_destroy(&js);
    shm_output_destroy(&shm);
    printf("ALL MODULE DESTROYED\n");
    return ret;
}