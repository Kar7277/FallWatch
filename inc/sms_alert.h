#ifndef SMS_ALERT_H
#define SMS_ALERT_H

typedef struct sms_alert_t{
    int fd;
    char phone[32];
    char uart_dev[64];
}sms_alert_t;
sms_alert_t* sms_alert_init(const char *uart_dev,const char *phone);
int sms_alert_send(sms_alert_t *sms,const char *msg);
void sms_alert_destroy(sms_alert_t *sms);
#endif