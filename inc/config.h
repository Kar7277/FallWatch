#ifndef CONFIG_H
#define CONFIG_H
typedef struct camera_config_t{
    char device_path[256];
    int width;
    int height;
    int fps;
    int buffer_count;
}camera_config_t;
typedef struct motion_config_t{
    int threshold;
    float alarm_ratio;
    int trigger_frames;
}motion_config_t;
typedef struct gpio_config_t{
    int led_pin;
}gpio_config_t;
typedef struct sms_config_t{
    char uart_dev[64];
    char phone[32];
}sms_config_t;
typedef struct mqtt_config_t{
    char broker_uri[256];
    char topic[128];
    char client_id[64];
    int keepalive;
}mqtt_config_t;
typedef struct lcd_config_t{
    char fb_dev[64];
    int refresh_ms;
}lcd_config_t;
typedef struct jpeg_config_t{
    char save_dir[256];
    int quality;
}jpeg_config_t;
typedef struct fallwatch_config_t{
      camera_config_t camera;
      motion_config_t motion;
      gpio_config_t   gpio;
      sms_config_t    sms;
      mqtt_config_t   mqtt;
      lcd_config_t    lcd;
      jpeg_config_t   jpeg;
  } fallwatch_config_t;
int  config_load(const char *path, fallwatch_config_t *cfg);
void config_set_defaults(fallwatch_config_t *cfg);
#endif