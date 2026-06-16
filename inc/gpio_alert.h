#ifndef GPIO_ALERT_H
#define GPIO_ALERT_H

typedef struct gpio_alert_t {
    int led_pin;        // LED GPIO 编号
    int led_exported;   // LED 是否已 export（用于逐级回滚）
    char value_path[64];
} gpio_alert_t;

int  gpio_alert_init(gpio_alert_t *gpio, int led_pin);
int gpio_led_on(gpio_alert_t *gpio);
int gpio_led_off(gpio_alert_t *gpio);
void gpio_alert_destroy(gpio_alert_t *gpio);

#endif
