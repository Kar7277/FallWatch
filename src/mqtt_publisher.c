#include "mqtt_publisher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int mqtt_init(mqtt_publisher_t *mq,const char* broker_uri,const char* topic,const char* client_id){
    if(mq == NULL || broker_uri == NULL || topic == NULL || client_id == NULL){
        fprintf(stderr,"INIT_MQTT IS NULL");
        return -1;
    }
    mq->connected = 0;
    strncpy(mq->broker_uri,broker_uri,sizeof(mq->broker_uri));
    strncpy(mq->topic,topic,sizeof(mq->topic));
    strncpy(mq->client_id,client_id,sizeof(mq->client_id));
    if(MQTTClient_create(&mq->client,mq->broker_uri,mq->client_id,MQTTCLIENT_PERSISTENCE_NONE,NULL)){
    /*int persistence 消息持久化方式——断线后未发出的消息存在哪 void *ctx持久化的上下文指针 MQTTCLIENT_PERSISTENCE_NONE（不持久化，嵌入式内存紧张*/
        fprintf(stderr,"CREATE CLIENT FAILED\n");
        return -1;
    }
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;//控制连接行为的结构体
    conn_opts.keepAliveInterval = 60;   // 心跳周期（秒），超时 broker 会断开
    if(MQTTClient_connect(mq->client,&conn_opts)){
        fprintf(stderr,"CONNECT CLIENT FAILED\n");
        MQTTClient_destroy(&mq->client);
        return -1;
    }
    mq->connected = 1;
    return 0;
}
int mqtt_publish_alert(mqtt_publisher_t *mq,const char* json_msg){
    if(mq == NULL){
        fprintf(stderr,"MQ IS NULL PUBLISH FAILED\n");
        return -1;
    }
    if(mq->connected == 0){
        return 0;
    }
    if(MQTTClient_publish(mq->client,mq->topic,strlen(json_msg),(void*)json_msg,1,0,NULL)){
        fprintf(stderr,"PUBLISHER MSG FAILED\n");
        return -1;
    }
    return 0;
}
void mqtt_destroy(mqtt_publisher_t *mq){
    if(mq == NULL){
        fprintf(stderr,"MQ IS NULL DESTROY FAILED\n");
        return;
    }
    if(mq->connected == 0){
        return;
    }
    if(MQTTClient_disconnect(mq->client,3000)){//3000ms 超时
        fprintf(stderr,"DISCONNECT CLIENT FAILED\n");
    }
    mq->connected = 0;
    MQTTClient_destroy(&mq->client);

}