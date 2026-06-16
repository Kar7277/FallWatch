# main.c 集成设计文档

**日期：** 2026-06-05
**类型：** 多模块集成
**状态：** 已批准

---

## 总体目标

将 4 个已验证模块（ring_buffer、v4l2_capture、capture_thread、motion_detect）加 4 个新模块（lcd_display、gpio_alert、mqtt_publisher、jpeg_save）集成到 `main.c`，形成完整的边缘监控系统。

## 执行顺序

严格按 1→11 顺序执行，每个模块写完验证后再进入下一个。

---

## 任务 1-2：ring_buffer 线程安全改造

**目标：** 让 ring_buffer 支持多线程并发访问（main.c 将有 3 个线程同时操作同一个 rb）。

**ring_buffer_t 新增字段：**

```c
pthread_mutex_t mutex;      // 保护 head/tail/count
pthread_cond_t  not_empty;  // 缓冲区非空信号
pthread_cond_t  not_full;   // 缓冲区非满信号
```

**接口变更：**

| 函数 | 变更 |
|------|------|
| `ring_buffer_init` | + `pthread_mutex_init` + 2× `pthread_cond_init` |
| `ring_buffer_destroy` | + `pthread_mutex_destroy` + 2× `pthread_cond_destroy` |
| `ring_buffer_put` | 加锁，满则 `cond_wait(&not_full)`，写入后 `cond_signal(&not_empty)` |
| `ring_buffer_get` | 加锁，空则 `cond_wait(&not_empty)`，取出后 `cond_signal(&not_full)` |
| `ring_buffer_count` | 加锁保护读 count |
| `ring_buffer_get_nonblock` | **新增**：非阻塞版本，空时立即返回 NULL（给 drain 循环用） |

**设计要点：**
- `put` 和 `get` 都用阻塞式——采集线程满了等消费者，消费者空了等生产者
- `get_nonblock` 专为 drain 场景设计——取到 NULL 就结束循环，不会永久阻塞
- 现有测试代码`不需要改`——加锁对单线程测试是透明开销

**影响文件：** `ring_buffer.h`、`ring_buffer.c`

---

## 任务 3-4：lcd_display 模块

**目标：** 从 `tools/test_uvc_lcd.c` 提取 LCD 显示逻辑为独立模块。

**源参考：** `tools/test_uvc_lcd.c` 的 `yuyv_to_rgb32()` + `/dev/fb0` 初始化 + mmap + 逐行拷贝

**lcd_display_t 结构体：**

| 字段 | 类型 | 用途 |
|------|------|------|
| `fd` | `int` | `/dev/fb0` 文件描述符 |
| `fb` | `unsigned char *` | mmap 显存指针 |
| `fb_size` | `size_t` | 显存总字节数 |
| `xres` / `yres` | `int` | LCD 物理分辨率 |
| `bpp` | `int` | 每像素位数 |
| `line_length` | `int` | 每行字节数 |
| `roff` / `goff` / `boff` | `int` | RGB 字节偏移（自动适配 XRGB/XBGR） |
| `src_width` / `src_height` | `int` | 输入帧分辨率 |

**对外接口：**

| 函数 | 返回 | 语义 |
|------|------|------|
| `lcd_init(lcd, fb_dev, src_w, src_h)` | int | 打开 fb0、获取参数、mmap、检测 RGB 布局。0 成功 / -1 失败 |
| `lcd_display_frame(lcd, frame)` | int | YUYV→RGB32 → 逐行搬 fb0。0 成功 / -1 参数错误 |
| `lcd_clear(lcd)` | void | 全屏清黑 |
| `lcd_destroy(lcd)` | void | munmap + close(fd) |

**YUYV→RGB32：** BT.601 定点运算（`>> 8`），`static` 内部函数

**显示位置：** 640×480 放 LCD 左上角（row 0~479, col 0~639），其余黑色

**错误处理：** `lcd_init` 失败返回 -1（允许无 LCD 运行检测+告警）；`lcd_display_frame` 高频调用不做 fprintf

**影响文件：** `lcd_display.h`（新建）、`lcd_display.c`（新建）

---

## 任务 5-6：gpio_alert 模块

**目标：** 通过 sysfs 控制 GPIO 引脚驱动 LED 闪烁 + 蜂鸣器鸣叫。

**硬件：** i.MX 6ULL GPIO（具体引脚号由用户确认），sysfs 路径 `/sys/class/gpio/`

**gpio_alert_t 结构体：**

| 字段 | 类型 | 用途 |
|------|------|------|
| `led_pin` | `int` | LED GPIO 编号 |
| `beep_pin` | `int` | 蜂鸣器 GPIO 编号 |
| `led_exported` | `int` | LED 是否已 export |
| `beep_exported` | `int` | 蜂鸣器是否已 export |

**对外接口：**

| 函数 | 语义 |
|------|------|
| `gpio_alert_init(alert, led_pin, beep_pin)` | export + 设方向为 out。0 成功 / -1 失败 |
| `gpio_led_on(alert)` / `gpio_led_off(alert)` | 写入 1 / 0 到 value |
| `gpio_beep_on(alert)` / `gpio_beep_off(alert)` | 写入 1 / 0 到 value |
| `gpio_alert_trigger(alert, duration_ms)` | LED 闪烁 + 蜂鸣器响 duration_ms 毫秒（阻塞式，简单用 usleep 实现） |
| `gpio_alert_destroy(alert)` | unexport + 清理 |

**设计要点：**
- sysfs 操作：`echo PIN > /sys/class/gpio/export` → `echo out > /sys/class/gpio/gpioPIN/direction` → `echo 1/0 > /sys/class/gpio/gpioPIN/value`
- `gpio_alert_trigger` 做简单阻塞式告警（LED 500ms 周期闪烁，蜂鸣器持续响），告警持续时间由 main.c 的检测线程调用者决定
- 告警是"必须成功"的关键路径——不走 QT，直连硬件

**影响文件：** `gpio_alert.h`（新建）、`gpio_alert.c`（新建）

---

## 任务 7-8：mqtt_publisher 模块

**目标：** 检测到运动/摔倒时，通过 MQTT 推送告警消息到手机。

**依赖：** libpaho-mqtt3c（C 版本），交叉编译产物放在 `lib/`

**mqtt_publisher_t 结构体：**

| 字段 | 类型 | 用途 |
|------|------|------|
| `client` | `MQTTClient *` | paho MQTT 客户端句柄 |
| `broker_uri` | `char[256]` | 如 `tcp://192.168.1.100:1883` |
| `topic` | `char[128]` | 如 `edgewatcher/alert` |
| `client_id` | `char[64]` | MQTT client ID |
| `connected` | `int` | 连接状态标志 |

**对外接口：**

| 函数 | 语义 |
|------|------|
| `mqtt_init(mq, broker_uri, topic, client_id)` | 初始化 MQTT 客户端，连接 broker。0 成功 / -1 失败 |
| `mqtt_publish_alert(mq, json_msg)` | PUBLISH 一条 JSON 消息到 topic。0 成功 / -1 失败 |
| `mqtt_destroy(mq)` | 断开连接、释放资源 |

**消息格式（JSON）：**

```json
{"type":"motion", "time":"2026-06-05 14:30:22", "motion_ratio":0.23, "img_size":0}
```

**设计要点：**
- MQTT 连接失败不应阻止系统启动（WiFi 可能没连上）——`mqtt_init` 失败时 `connected=0`，后续 publish 静默跳过
- paho MQTT C 库：`MQTTClient_create` → `MQTTClient_connect` → `MQTTClient_publish`
- 用 QoS 1（至少一次送达）

**影响文件：** `mqtt_publisher.h`（新建）、`mqtt_publisher.c`（新建）

---

## 任务 9-10：jpeg_save 模块

**目标：** 告警触发时将当前帧压缩为 JPEG 并保存到本地作为证据。

**依赖：** libjpeg（交叉编译产物放在 `lib/`）

**目录：** `/var/edgewatcher/`（需 `mkdir -p` 创建）

**jpeg_save_t 结构体：**

| 字段 | 类型 | 用途 |
|------|------|------|
| `save_dir` | `char[256]` | 保存目录路径 |
| `quality` | `int` | JPEG 压缩质量（1-100） |
| `width` / `height` | `int` | 帧分辨率 |

**对外接口：**

| 函数 | 语义 |
|------|------|
| `jpeg_save_init(js, save_dir, width, height, quality)` | 创建保存目录。0 成功 / -1 失败 |
| `jpeg_save_frame(js, frame)` | YUYV→JPEG 压缩并写入文件（文件名为 `alarm_YYYYMMDD_HHMMSS.jpg`）。0 成功 / -1 失败 |
| `jpeg_save_destroy(js)` | 清理（无特殊资源需释放） |

**压缩流程：**
1. `jpeg_std_error` + `jpeg_create_compress`
2. `jpeg_stdio_dest` → 目标文件
3. 设置参数：`image_width/height = width/height`，`input_components = 3`，`in_color_space = JCS_YCbCr`
4. 逐行 `jpeg_write_scanlines`：YUYV → 提取 Y/Cb/Cr 三个平面 → 传入 libjpeg
5. `jpeg_finish_compress` + `jpeg_destroy_compress`

**设计要点：**
- libjpeg 直接接受 YCbCr 色彩空间，YUYV 的 Y 可以直接用，但 U/V 需要插值（4:2:2→4:4:4 色度上采样，每两个像素共享一组 UV）
- 文件名含时间戳做唯一标识
- 保存失败不影响告警主流程

**影响文件：** `jpeg_save.h`（新建）、`jpeg_save.c`（新建）

---

## 任务 11：main.c 多线程集成

**目标：** 三个线程 + 主线程协调，形成完整的采集→显示→检测→告警流水线。

### 线程架构

```
主线程 (main)
  │
  ├─ 初始化所有模块
  ├─ 启动采集线程
  ├─ 启动显示线程
  ├─ 启动检测线程
  ├─ 等待 SIGINT / SIGTERM
  ├─ 停止所有线程
  ├─ drain 残留帧
  └─ 销毁所有模块

采集线程 (capture_thread)          显示线程 (display_thread)        检测线程 (detect_thread)
─────────────────────              ────────────────────            ────────────────────
while(running):                    while(running):                  while(running):
  poll(fd, 2000ms)                   frame = get(rb)  //阻塞         frame = get(rb)  //阻塞
  DQBUF                              无帧(超时/停止)→continue       无帧(超时/停止)→continue
  malloc frame_t                     lcd_display_frame(frame)       result = motion_detect_process(frame)
  memcpy data                        frame_free(frame)              if result==1:
  ring_buffer_put(rb, frame)                                           gpio_alert_trigger(5s)
  QBUF                                                                  mqtt_publish_alert(...)
                                                                       jpeg_save_frame(frame)
                                                                    frame_free(frame)
```

### 线程间数据流

```
摄像头 ──→ 采集线程 ──put──→ RingBuffer ──get──→ 显示线程 ──→ LCD
                              (加锁)       ──get──→ 检测线程 ──→ GPIO/BEEP
                                                                  ├─→ MQTT
                                                                  └─→ JPEG
```

### main.c 伪代码结构

```c
#include 所有模块头文件
#include <signal.h>

// 全局变量（信号处理器用）
static capture_thread_ctx *g_cap_ctx = NULL;  // 或直接用 volatile int g_running

static void sig_handler(int sig) {
    // 设置停止标志
}

int main() {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 1. 初始化 ring_buffer（capacity=8）
    // 2. 初始化 V4L2 + capture_thread
    // 3. 初始化 motion_detect
    // 4. 初始化 lcd_display（失败只打印警告，不退出）
    // 5. 初始化 gpio_alert
    // 6. 初始化 mqtt_publisher（失败只打印警告）
    // 7. 初始化 jpeg_save

    // 8. 启动采集线程
    // 9. 启动显示线程
    // 10. 启动检测线程

    // 11. 等待信号（pause() 或 while(g_running) sleep(1)）

    // 12. 停止采集线程（先停，避免继续生产）
    // 13. 停止显示线程
    // 14. 停止检测线程

    // 15. drain 环形缓冲区残留帧（get_nonblock）

    // 16. 销毁所有模块（逆序：先创建的后销毁）

    return 0;
}
```

### display_thread 和 detect_thread 实现

两个线程函数写在 `main.c` 里（不需要独立 .h/.c），因为它们只是从 rb 取帧 + 调模块接口的薄胶水层。

**显示线程关键逻辑：**
- 阻塞 `ring_buffer_get(rb)` 取帧
- 调 `lcd_display_frame(lcd, frame)`
- `frame_free(frame)`
- 如果 `rb->running` 变为 0 且 rb 已空，get 返回 NULL → 退出循环

**检测线程关键逻辑：**
- 阻塞 `ring_buffer_get(rb)` 取帧（不需要每帧都检测，可以每 3 帧取一次 = 约 7fps 检测率）
- 调 `motion_detect_process(md, frame)` 
- 返回 1 → `gpio_alert_trigger()` + `mqtt_publish_alert()` + `jpeg_save_frame()`
- `frame_free(frame)`

### 抽帧策略

检测线程不需要处理每一帧（30fps 检测浪费 CPU）。方案：
- 每 3 帧处理 1 帧（mod frame->id % 3 == 0）
- 简单有效，不需要定时器

### 停止流程（关键！）

停止顺序必须是：**采集线程先停 → 显示/检测线程后停**。

原因：采集线程是唯一的生产者。如果先停消费者，生产者继续 put 会填满缓冲区然后阻塞。先停生产者，消费者 drain 完残留帧后自然退出。

修改 `ring_buffer_get`：为支持停止信号，需要额外机制。方案：
- 在 `ring_buffer_t` 加 `volatile int active` 字段
- `ring_buffer_stop(rb)` 设 `active=0` + `pthread_cond_broadcast` 唤醒所有等待者
- `ring_buffer_get` 在 `active=0` 且缓冲区空时返回 NULL（而非继续阻塞等待）

### 错误处理策略

| 模块 | 初始化失败 | 运行时失败 |
|------|-----------|-----------|
| V4L2/capture | 致命，退出程序 | 致命，退出程序 |
| ring_buffer | 致命，退出程序 | 致命，退出程序 |
| motion_detect | 致命，退出程序 | 打印错误，跳过该帧 |
| lcd_display | **警告，继续运行** | 打印错误，跳过该帧 |
| gpio_alert | **警告，继续运行** | 打印错误，静默 |
| mqtt_publisher | **警告，继续运行** | 打印错误，静默 |
| jpeg_save | **警告，继续运行** | 打印错误，静默 |

核心原则：采集+检测是必须的；LCD/MQTT/GPIO/JPEG 是辅助功能，挂了不影响核心监控。

---

## 完整文件清单

```
do_my_test/
├── ring_buffer.h              🔧 加锁 + cond + get_nonblock
├── ring_buffer.c              🔧 加锁实现
├── v4l2_capture.h             ✅ 不变
├── v4l2_capture.c             ✅ 不变
├── capture_thread.h           ✅ 不变
├── capture_thread.c           ✅ 不变
├── motion_detect.h            ✅ 不变
├── motion_detect.c            ✅ 不变
├── lcd_display.h              ✨ 新建
├── lcd_display.c              ✨ 新建
├── gpio_alert.h               ✨ 新建
├── gpio_alert.c               ✨ 新建
├── mqtt_publisher.h           ✨ 新建
├── mqtt_publisher.c           ✨ 新建
├── jpeg_save.h                ✨ 新建
├── jpeg_save.c                ✨ 新建
├── main.c                     ✨ 新建（从 4 行 #include 扩到完整程序）
├── CMakeLists.txt             ✨ 新建（交叉编译构建脚本）
├── test_*.c                   ✅ 不变（已有测试保留）
└── .vscode/                   ✅ 不变
```
