#include "mqtt_publisher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
    //-------------test 1 ---------
    mqtt_publisher_t mq;
    char *broker_url = "tcp://192.168.99.99:1883"; 
    char *topic = "edgewatcher/test";
    char *client_id = "test_mqtt_invalid";
    if(mqtt_init(&mq,broker_url,topic,client_id) == -1){
        printf("TEST_1_INIT SUCCESS\n");
    }
    else{
        printf("TEST_1_INIT FAILED\n");
    }
    char *msg = "test test";
    mqtt_publish_alert(&mq,msg);
    mqtt_destroy(&mq);

    //-------------test 2 ---------
    broker_url = "tcp://127.0.0.1:1883"; 
    topic = "edgewatcher/test";
    client_id = "test_mqtt_valid";
    if(mqtt_init(&mq,broker_url,topic,client_id) == 0 && mq.connected == 1){
        printf("TEST_2_INIT SUCCESS\n");
    }
    else{
        printf("TEST_2_INIT FAILED\n");
    }
    mqtt_publish_alert(&mq,msg);
    mqtt_destroy(&mq);
    //-------------test 3 ---------
    mqtt_destroy(&mq);
    printf("TEST IS OVER\n");
    return 0;
}
