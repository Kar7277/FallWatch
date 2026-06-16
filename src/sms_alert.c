#include "sms_alert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
sms_alert_t* sms_alert_init(const char *uart_dev,const char *phone){   
    sms_alert_t *sms = (sms_alert_t*)malloc(sizeof(sms_alert_t));
    if(sms == NULL){
        fprintf(stderr,"MALLOC STRUCT FAILED");
        return NULL;
    }
    memset(sms,0,sizeof(*sms));
    strncpy(sms->uart_dev,uart_dev,sizeof(sms->uart_dev));
    strncpy(sms->phone,phone,sizeof(sms->phone));
    int fd = open(uart_dev,O_RDWR | O_NOCTTY | O_NONBLOCK); 
    if(fd < 0){
        fprintf(stderr,"OPEN UART FAILED");
        free(sms);
        return NULL;
    }
    sms->fd = fd;
    struct termios opt;
    if(tcgetattr(sms->fd,&opt) == -1){
        close(sms->fd);
        free(sms);
        return NULL;
    }
    cfsetispeed(&opt,B115200);
    cfsetospeed(&opt,B115200);
    opt.c_cflag &= ~(CRTSCTS | PARENB | CSTOPB |CSIZE);
    opt.c_cflag |= (CS8 | CLOCAL);
    opt.c_lflag &= ~(ECHO | ICANON | ISIG);
    opt.c_cc[VTIME] = 10;
    opt.c_cc[VMIN] = 0;
    if(tcsetattr(sms->fd,TCSANOW,&opt) == -1){
        close(sms->fd);
        free(sms);
        return NULL;
    }
    int flags = fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags & ~O_NONBLOCK);
    char *buf = "AT\r\n";
    write(sms->fd,buf,strlen(buf));
    usleep(200000);
    //读回复
    char resp[128];
    int n = read(sms->fd,resp,sizeof(resp)-1);
    if(n <= 0){
        close(sms->fd);
        free(sms);
        return NULL;
    }
    resp[n] = '\0';
    if(!strstr(resp,"OK")){
        close(sms->fd);
        free(sms);
        return NULL;
    }
    return sms;
}

static int uart_cmd(int fd,const char *cmd,const char *expected){
    write(fd,cmd,strlen(cmd));
    usleep(200000);
    char resp[128];
    int n = read(fd,resp,sizeof(resp)-1);
    if(n <= 0){
        fprintf(stderr,"READ UART_CMD_FD FAILED");
        return -1;
    }
    resp[n] = '\0';
    if(strstr(resp,expected)){
        return 0;
    }
    return -1;
}
int sms_alert_send(sms_alert_t *sms,const char *msg){
    char *buf = "AT+CMGF=1\r\n";
    if(uart_cmd(sms->fd,buf,"OK")){
        fprintf(stderr,"SET TEXT MODE FAILED");
        return -1;
    }
    char cmdbuf[64];
    snprintf(cmdbuf,sizeof(cmdbuf),"AT+CMGS=\"%s\"\r\n",sms->phone);
    if(uart_cmd(sms->fd,cmdbuf,"> ")){
        fprintf(stderr,"SET RECEIVER FAILED");
        return -1;
    }
    write(sms->fd,msg,strlen(msg));
    write(sms->fd,"\x1A",1);
    usleep(1000000);
    char resp[128];
    int n = read(sms->fd,resp,sizeof(resp)-1);
    if(n <= 0){
        fprintf(stderr,"READ SEND_FD FAILED");
        close(sms->fd);
        return -1;
    }
    resp[n] = '\0';
    char *expected = "+CMGS:";
    if(!strstr(resp,expected)){
        fprintf(stderr,"SEND TEXT FAILED");
        return -1;
    }
    expected = "OK";
    if(!strstr(resp,expected)){
        fprintf(stderr,"RECEIVE 'OK' FAILED");
        return -1;
    }
    return 0;

}
void sms_alert_destroy(sms_alert_t *sms){
    if(sms != NULL && sms->fd >= 0){
        close(sms->fd);
        sms->fd = -1;
    }
    if(sms != NULL){
        free(sms);
    }
}