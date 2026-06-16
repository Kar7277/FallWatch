#include "gpio_alert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gpio_alert_init(gpio_alert_t *gpio, int led_pin){
    if(gpio == NULL){
        fprintf(stderr,"GPIO IS NULL\n");
        return -1;
    }
    
    FILE* fp_export = fopen("/sys/class/gpio/export","w"); 
    if(!fp_export){
        fprintf(stderr,"OPEN FILE_EXPORT FAILED\n");
        return -1;
    }
    if(fprintf(fp_export,"%d",led_pin) < 0){
        perror("fprintf export");
        fclose(fp_export);
        return -1;
    }
    fclose(fp_export);
    char direction_path[64];
    snprintf(direction_path,64,"/sys/class/gpio/gpio%d/direction",led_pin);
    FILE* fp_direction = fopen(direction_path,"w");
    if(!fp_direction){
        fprintf(stderr,"SET DIRECTION FAILED\n");
        FILE* fp_unexport = fopen("/sys/class/gpio/unexport","w"); 
        if(!fp_unexport){
            fprintf(stderr,"OPEN FILE_UNEXPORT FAILED\n");
            return -1;
        }
        if(fprintf(fp_unexport,"%d",led_pin) < 0){
            perror("fprintf unexport");
        }
        fclose(fp_unexport);
        return -1;
    }
    if(fprintf(fp_direction,"out") < 0){
        perror("fprintf direction");
        fclose(fp_direction);
        return -1;
    }
    fclose(fp_direction);

    snprintf(gpio->value_path,64,"/sys/class/gpio/gpio%d/value",led_pin);
    gpio->led_pin = led_pin;
    gpio->led_exported = 1;
    return 0;
}
int gpio_led_on(gpio_alert_t *gpio){
    if(gpio == NULL){
        fprintf(stderr,"LED_ON_GPIO IS NULL\n");
        return -1;
    }
    FILE* fp = fopen(gpio->value_path,"w");
    if(!fp){
        perror("fopen value_path");
        return -1;
    }
    if(fprintf(fp,"0") < 0){
        perror("fprintf value 0");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}
int gpio_led_off(gpio_alert_t *gpio){
    if(gpio == NULL){
        fprintf(stderr,"LED_OFF_GPIO IS NULL\n");
        return -1;
    }
    FILE* fp = fopen(gpio->value_path,"w");
    if(!fp){
        perror("fopen value_path");
        return -1;
    }
    if(fprintf(fp,"1") < 0){
        perror("fprintf value 1");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}
void gpio_alert_destroy(gpio_alert_t *gpio){
    if(gpio == NULL){
        fprintf(stderr,"DESTROY_GPIO IS NULL\n");
        return;
    }
    if(gpio->led_exported == 1){
        FILE* fp_unexport = fopen("/sys/class/gpio/unexport","w"); 
        if(!fp_unexport){
            fprintf(stderr,"OPEN FILE_UNEXPORT FAILED\n");
        }
        else{
            if(fprintf(fp_unexport,"%d",gpio->led_pin) < 0){
                perror("fprintf unexport");
            }
            fclose(fp_unexport);
        }
    }
    gpio->led_pin = -1;
    gpio->led_exported = 0;
    memset(gpio->value_path,0,sizeof(gpio->value_path));
}