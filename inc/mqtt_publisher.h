#ifndef MQTT_PUBLISHER_H
#include <MQTTClient.h>
#define MQTT_PUBLISHER_H
typedef struct mqtt_publisher_t{
    MQTTClient client;
    char broker_uri[256];
    char topic[256];
    char client_id[64];
    int connected;
}mqtt_publisher_t;
int mqtt_init(mqtt_publisher_t *mq,const char* broker_uri,const char* topic,const char* client_id);
int mqtt_publish_alert(mqtt_publisher_t *mq,const char* json_msg);
void mqtt_destroy(mqtt_publisher_t *mq);
#endif