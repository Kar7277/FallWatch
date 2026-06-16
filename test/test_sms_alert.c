#include "sms_alert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void){
    //---------------TEST 1--------无效串口设备
    char* uart_dev = "/dev/ttymxc666";
    char* phone = "18784612053";
    sms_alert_t* sms = sms_alert_init(uart_dev,phone);
    if(sms == NULL){
        printf("TEST_1_INIT SUCCESS\n");
    }
    else{
        printf("TEST_1_INIT FAILED\n");
    }
    sms_alert_destroy(sms);
    //---------------TEST 2--------有效串口 + AT 握手
    uart_dev = "/dev/ttymxc5";
    sms = sms_alert_init(uart_dev,phone);
    if(sms == NULL){
        printf("TEST_2_INIT FAILED\n");
    }
    else{
        printf("TEST_2_INIT SUCCESS\n");
    }
    sms_alert_destroy(sms);
    sms = NULL;
    // char* msg = "test test";
    // if(sms_alert_send(sms,msg) == -1){
    //     printf("SMS SEND FAILED\n ");
    // }
    
    //---------------TEST 3--------双重 destroy
    sms_alert_destroy(sms);
    printf("TEST_3_INIT SUCCESS\n");
    return 0;

}