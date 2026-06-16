#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

/* ---------- 辅助：切掉行末的 \n 和 \r ---------- */
static void trim_newline(char *s)
{
    char *p = strchr(s, '\n');
    if (p) *p = '\0';
    p = strchr(s, '\r');
    if (p) *p = '\0';
}

/* ---------- 辅助：切掉字符串末尾的空格/tab ---------- */
static void trim_head_spaces(char *s)
{
    char *src = s;
    while (*src == ' ' || *src == '\t') src++;
    if (src != s) memmove(s, src, strlen(src) + 1);
}

static void trim_tail_spaces(char *s)
{
    char *p = s + strlen(s) - 1;
    while (p >= s && (*p == ' ' || *p == '\t')) {
        *p = '\0';
        p--;
    }
}

void config_set_defaults(fallwatch_config_t *cfg)
{
    /* camera */
    strcpy(cfg->camera.device_path, "/dev/video1");
    cfg->camera.width        = 640;
    cfg->camera.height       = 480;
    cfg->camera.fps          = 30;
    cfg->camera.buffer_count = 4;

    /* motion */
    cfg->motion.threshold      = 25;
    cfg->motion.alarm_ratio    = 0.15f;
    cfg->motion.trigger_frames = 3;

    /* mqtt */
    strcpy(cfg->mqtt.broker_uri, "tcp://192.168.1.100:1883");
    strcpy(cfg->mqtt.topic,      "fallwatch/alert");
    strcpy(cfg->mqtt.client_id,  "fallwatch_001");
    cfg->mqtt.keepalive = 60;

    /* sms */
    strcpy(cfg->sms.uart_dev, "/dev/ttymxc5");
    strcpy(cfg->sms.phone,     "+8618784612053");

    /* gpio */
    cfg->gpio.led_pin = 129;

    /* lcd */
    strcpy(cfg->lcd.fb_dev, "/dev/fb0");
    cfg->lcd.refresh_ms = 200;

    /* jpeg */
    strcpy(cfg->jpeg.save_dir, "./snapshots");
    cfg->jpeg.quality = 80;
}

int config_load(const char *path, fallwatch_config_t *cfg)
{
    FILE *fp;
    char  line[512];
    char  section[64] = "";
    char  key[128], value[256];

    /* 1. 先填默认值，文件里的配置覆盖默认值 */
    config_set_defaults(cfg);

    /* 2. 打开 ini 文件 */
    fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "[config] open %s failed, using defaults\n", path);
        return -1;
    }

    /* 3. 逐行解析 */
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);

        /* 跳过空行 */
        if (line[0] == '\0') continue;

        /* 跳过注释（; 或 # 开头） */
        if (line[0] == ';' || line[0] == '#') continue;

        /* --- 段名行: [xxxx] --- */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strcpy(section, line + 1);
            }
            continue;
        }

        /* --- 键值行: key = value --- */
        if (sscanf(line, "%127[^=]= %255[^\n]", key, value) == 2) {
            trim_head_spaces(key);
            trim_tail_spaces(key);

            /* ---- camera ---- */
            if (strcmp(section, "camera") == 0) {
                if      (strcmp(key, "device_path")  == 0) strcpy(cfg->camera.device_path, value);
                else if (strcmp(key, "width")        == 0) cfg->camera.width        = atoi(value);
                else if (strcmp(key, "height")       == 0) cfg->camera.height       = atoi(value);
                else if (strcmp(key, "fps")          == 0) cfg->camera.fps          = atoi(value);
                else if (strcmp(key, "buffer_count") == 0) cfg->camera.buffer_count = atoi(value);
            }
            /* ---- motion ---- */
            else if (strcmp(section, "motion") == 0) {
                if      (strcmp(key, "threshold")      == 0) cfg->motion.threshold      = atoi(value);
                else if (strcmp(key, "alarm_ratio")    == 0) cfg->motion.alarm_ratio    = (float)atof(value);
                else if (strcmp(key, "trigger_frames") == 0) cfg->motion.trigger_frames = atoi(value);
            }
            /* ---- mqtt ---- */
            else if (strcmp(section, "mqtt") == 0) {
                if      (strcmp(key, "broker_uri") == 0) strcpy(cfg->mqtt.broker_uri, value);
                else if (strcmp(key, "topic")      == 0) strcpy(cfg->mqtt.topic,      value);
                else if (strcmp(key, "client_id")  == 0) strcpy(cfg->mqtt.client_id,  value);
                else if (strcmp(key, "keepalive")  == 0) cfg->mqtt.keepalive = atoi(value);
            }
            /* ---- sms ---- */
            else if (strcmp(section, "sms") == 0) {
                if      (strcmp(key, "uart_dev") == 0) strcpy(cfg->sms.uart_dev, value);
                else if (strcmp(key, "phone")    == 0) strcpy(cfg->sms.phone,    value);
            }
            /* ---- gpio ---- */
            else if (strcmp(section, "gpio") == 0) {
                if      (strcmp(key, "led_pin") == 0) cfg->gpio.led_pin = atoi(value);
            }
            /* ---- lcd ---- */
            else if (strcmp(section, "lcd") == 0) {
                if      (strcmp(key, "fb_dev")     == 0) strcpy(cfg->lcd.fb_dev, value);
                else if (strcmp(key, "refresh_ms") == 0) cfg->lcd.refresh_ms = atoi(value);
            }
            /* ---- jpeg ---- */
            else if (strcmp(section, "jpeg") == 0) {
                if      (strcmp(key, "save_dir") == 0) strcpy(cfg->jpeg.save_dir, value);
                else if (strcmp(key, "quality")  == 0) cfg->jpeg.quality = atoi(value);
            }
        }
    }

    fclose(fp);
    return 0;
}
