# FallWatch — 边缘计算跌倒检测系统

> 运行在 ARM Linux 嵌入式平台的实时智能跌倒检测系统。
> **V4L2 视频采集 + 帧差法运动检测 + 多重告警联动 + Qt GUI 监控界面**
> 实现从"视频采集 → 实时预览 → 跌倒检测 → 告警推送"的完整闭环。

---

## 目录

- [1. 项目速览](#1-项目速览)
- [2. 仓库结构](#2-仓库结构)
- [3. 系统架构](#3-系统架构)
- [4. 核心模块详解](#4-核心模块详解)
- [5. 数据流转](#5-数据流转)
- [6. 摔倒检测算法](#6-摔倒检测算法)
- [7. 硬件要求](#7-硬件要求)
- [8. 环境依赖与构建](#8-环境依赖与构建)
- [9. 配置说明](#9-配置说明)
- [10. 运行与测试](#10-运行与测试)
- [11. Qt GUI 界面](#11-qt-gui-界面)
- [12. 设计决策与 FAQ](#12-设计决策与-faq)

---

## 1. 项目速览

### 这是什么项目？

`FallWatch` 是面向独居老人场景的嵌入式智能监控系统，特点是：

- **实时视频采集**：V4L2 + mmap 零拷贝，YUYV 格式
- **本地智能检测**：纯 C 手写帧差法 + 状态机，不依赖 OpenCV
- **多重告警联动**：MQTT 远程推送 + AIR780E 短信 + GPIO LED 闪烁
- **证据留存**：触发时自动 JPEG 截图保存
- **本地预览**：Framebuffer 直写 LCD 实时显示
- **桌面监控**：Qt C++ 桌面 GUI，通过共享内存获取实时画面

### 解决了什么问题？

- 独居老人跌倒后无法主动求救的问题
- 纯云端方案对网络依赖强、延迟高的问题（边缘端自主检测）
- 嵌入式平台资源受限下的算法实现（不依赖 OpenCV 等重型库）

---

## 2. 仓库结构

```text
FallWatch/
├── src/                    # 主程序源码（14 个模块）
│   ├── main.c              #   入口：初始化各模块，主循环
│   ├── v4l2_capture.c      #   V4L2 摄像头采集（mmap 模式）
│   ├── ring_buffer.c       #   线程安全环形缓冲区
│   ├── capture_thread.c    #   采集线程管理
│   ├── motion_detect.c     #   帧差法运动检测 + 状态机
│   ├── lcd_display.c       #   Framebuffer LCD 显示
│   ├── gpio_alert.c        #   sysfs GPIO LED 控制
│   ├── sms_alert.c         #   AT 指令短信告警
│   ├── mqtt_publisher.c    #   MQTT 消息推送
│   ├── jpeg_save.c         #   libjpeg 压缩保存截图
│   ├── shm_output.c        #   共享内存输出（给 Qt GUI）
│   ├── yuyv_to_rgb_neon.c  #   ARM NEON 加速色彩转换
│   ├── config.c            #   INI 配置文件解析
│   └── Makefile            #   交叉编译脚本
├── inc/                    # 头文件（14 个）
├── gui/                    # Qt GUI 监控界面
│   └── fallwatch_gui/      #   Qt Widgets 项目
├── test/                   # 模块单元测试（7 个）
├── tools/                  # 辅助工具
├── scripts/                # 启动脚本
├── lib/                    # 预编译第三方库
├── config.ini.example      # 配置文件模板
└── docs/                   # 设计文档（本地保留）
```

---

## 3. 系统架构

### 整体架构图

```text
                        ┌─────────────────────┐
                        │     Qt 桌面 GUI      │
                        │  状态灯 / 日志 / 预览 │
                        └──────────┬──────────┘
                                   │ 读共享内存
                    ┌──────────────┴──────────────┐
                    │     共享内存 (shm_output)     │
                    │    RGB 帧 + 事件标志位        │
                    └──────────────┬──────────────┘
                                   │
    ┌──────────────────────────────┼──────────────────────────────┐
    │                    环形缓冲区 (Ring Buffer)                   │
    │              互斥锁 mutex + 条件变量 cond                     │
    └──┬───────────────────────────┼──────────────────────────────┘
       │                           │
  ┌────▼────────┐           ┌─────▼──────────┐
  │ 采集线程     │           │ 检测线程         │
  │ V4L2 + mmap │           │ 帧差法 + 状态机   │
  │ YUYV → RGB  │           │ 告警联动          │
  └────┬────────┘           └─────┬──────────┘
       │                          │
       ▼                    ┌─────┼──────┐
  [USB 摄像头]              ▼     ▼       ▼
                     ┌─────────┐ ┌──────────┐ ┌──────────────┐
                     │ LED 闪烁 │ │ AIR780E  │ │ MQTT → 云端  │
                     │ GPIO 控制│ │ 短信推送  │ │ 远程推送      │
                     └─────────┘ └──────────┘ └──────────────┘
```

### 职责划分（核心原则）

- **采集线程**：V4L2 零拷贝取帧 → 写入环形缓冲区。只管取，不管处理
- **检测线程**：从环形缓冲区抽帧 → 帧差法检测 → 触发告警。关键路径走最短链路
- **Qt GUI**：从共享内存读 RGB 预览帧 → 渲染状态灯/日志。不参与告警关键路径
- **主线程**：初始化各模块 → YUYV→RGB 转换 → LCD 实时预览 → 协调告警模式切换

---

## 4. 核心模块详解

### 4.1 视频采集模块 (`v4l2_capture.c`)

**技术**：Linux V4L2 API + mmap 内存映射

| 项目 | 说明 |
|------|------|
| 设备 | `/dev/video1` (UVC 摄像头) |
| 格式 | YUYV 4:2:2，640×480 @30fps |
| 缓冲区 | 4 个内核缓冲区，mmap 映射到用户空间 |
| 关键接口 | `v4l2_init()` / `v4l2_start()` / `v4l2_dequeue()` / `v4l2_destroy()` |

**为什么用 mmap 而不是 read()？**

- `read()` 每次把帧数据从内核拷贝到用户空间，多一次 memcpy
- `mmap` 直接把内核 DMA 缓冲区映射到用户空间，**零拷贝**
- 640×480×2 = 614KB/帧，30fps = 18MB/s，零拷贝的意义巨大
- i.MX 6ULL 单核 ARM Cortex-A7，能省一点是一点

**为什么用 YUYV 而不是 MJPEG？**

- YUYV 不需要硬件解码，CPU 直接处理
- MJPEG 虽然带宽小，但在 i.MX 6ULL 上软件解码很吃力
- 帧差法需要 Y（亮度）分量，YUYV 可以直接取出 Y 平面，不需要额外解压

---

### 4.2 环形缓冲区 (`ring_buffer.c`)

**技术**：固定大小环形数组 + `pthread_mutex_t` + `pthread_cond_t`

```c
typedef struct {
    frame_t        *frames;      // 帧数组
    int             capacity;    // 最大帧数（默认 8）
    int             read_pos;    // 读位置
    int             write_pos;   // 写位置
    int             count;       // 当前可用帧数
    volatile int    stop;        // 停止标志
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;   // 消费者等待信号
    pthread_cond_t  not_full;    // 生产者等待信号
} ring_buffer_t;
```

**为什么用环形缓冲区？**

- 采集线程和检测线程速度不匹配（30fps 采集 ≠ 10fps 检测）
- 解耦生产者和消费者，各自按自己的节奏跑
- 固定大小 = 无动态分配，无内存碎片，适合长期运行

**为什么用互斥锁 + 条件变量，而不是无锁队列？**

- i.MX 6ULL 是单核 CPU，无锁队列的优势发挥不出来
- 条件变量让消费者在队列空时阻塞等待（不空转烧 CPU）

---

### 4.3 运动检测模块 (`motion_detect.c`)

**技术**：Y 分量帧差法 + 滑动窗口阈值判定 + 状态机

核心流程详见 [§6 摔倒检测算法](#6-摔倒检测算法)。

---

### 4.4 告警联动模块

检测到跌倒后，**同时触发以下动作**（关键路径走最短链路）：

| 动作 | 实现 | 模块 |
|------|------|------|
| GPIO LED 闪烁 | `sysfs write /sys/class/gpio` | `gpio_alert.c` |
| SMS 短信推送 | UART AT 指令控制 AIR780E | `sms_alert.c` |
| MQTT 远程推送 | paho-mqtt C 库 | `mqtt_publisher.c` |
| JPEG 截图保存 | libjpeg-turbo 压缩 | `jpeg_save.c` |
| GUI 状态更新 | 共享内存事件标志位 | `shm_output.c` |

**为什么告警不经过 Qt，而是直接操作 GPIO？**

- GPIO 告警是"必须成功"的操作，不能因 Qt 进程卡顿而延迟
- 检测线程直连硬件，Qt 只是"收到通知后更新 UI"
- 这是嵌入式系统的铁律：**关键路径走最短链路**

---

### 4.5 色彩转换与 NEON 加速 (`yuyv_to_rgb_neon.c`)

**技术**：BT.601 色彩空间转换 + ARM NEON SIMD 指令集

- 默认使用纯 C 实现 YUYV → RGB 转换（BT.601 公式）
- 当编译时定义 `__ARM_NEON__`，自动启用 NEON SIMD 并行加速
- NEON 版本一次处理 8 个像素，大幅降低色彩转换的 CPU 占用

---

### 4.6 LCD 显示模块 (`lcd_display.c`)

**技术**：Linux Framebuffer (`/dev/fb0`)，直接写显存

- 根据 `fb_var_screeninfo` 自动检测 RGB 布局（32bpp）
- 直接 mmap 显存，CPU 写进去屏幕就显示，零中间层
- 支持区域清除 `lcd_clear()` —— 只覆盖变化区域，减少写屏开销

---

### 4.7 配置管理 (`config.c`)

**技术**：自写轻量 INI 解析器，无第三方依赖

支持 6 个配置段：

| 段 | 配置项 |
|----|--------|
| `[camera]` | 设备路径、分辨率、帧率、缓冲区数量 |
| `[motion]` | 帧差阈值、运动占比、触发帧数 |
| `[mqtt]` | Broker 地址、主题、客户端 ID、心跳 |
| `[sms]` | UART 设备路径、目标手机号 |
| `[gpio]` | LED 引脚编号 |
| `[lcd]` | Framebuffer 设备、刷新间隔 |
| `[jpeg]` | 保存目录、压缩质量 |

---

### 4.8 共享内存输出 (`shm_output.c`)

**用途**：主线程向 Qt GUI 传输实时预览画面和事件

- RGB 帧数据：主线程每帧写入，GUI 端 100ms 定时读取（无锁，允许偶尔撕裂）
- 事件标志位：warning / cancelled 标志供 GUI 读取
- GUI 通过 cancel 标记通知主线程退出报警模式

---

## 5. 数据流转

```text
[USB 摄像头]
     │ V4L2 ioctl + mmap
     ▼
【采集线程】 ──── YUYV 帧 ────→ 环形缓冲区
     │
     ▼
【主线程】
     ├─ 1) 从环形缓冲区取帧
     ├─ 2) YUYV → RGB（NEON 加速）
     ├─ 3) 写入共享内存 ──→ Qt GUI 预览
     ├─ 4) 写入 LCD Framebuffer
     └─ 5) 送入运动检测模块
              │
              ▼ (检测到跌倒)
         【告警联动】
         ├── GPIO LED 闪烁
         ├── SMS 短信发送
         ├── MQTT 消息推送
         └── JPEG 截图保存
```

---

## 6. 摔倒检测算法

### 算法流程

```
第 1 步：抽帧
  - 检测每 300ms 从环形缓冲区取一帧（不处理每一帧，省 CPU）

第 2 步：提取 Y 分量
  - YUYV 中，Y 在 byte[0] 和 byte[2]（每 4 字节一组）
  - 生成 640×480 单通道 Y 平面

第 3 步：帧差计算
  - |Y_curr[i] - Y_prev[i]| > THRESHOLD → 该像素为"变化点"
  - 统计变化点总数

第 4 步：运动判定
  - 变化点数 / 总像素数 = motion_ratio
  - motion_ratio > alarm_ratio → 判定为"有运动"

第 5 步：摔倒判定（状态机）
  IDLE ──(motion_ratio > 阈值)──▶ MOTION
  MOTION ──(连续 N 帧)─────────▶ FALL_EVENT → 触发告警
```

### 状态机设计

```
  ┌──────────┐
  │          │
  │   IDLE   │ ← 无运动，等待中
  │          │
  └────┬─────┘
       │ motion_ratio > alarm_ratio
       ▼
  ┌──────────┐
  │          │
  │  MOTION  │ ← 检测到运动
  │          │
  └────┬─────┘
       │ 连续 trigger_frames 帧都满足条件
       ▼
  ┌──────────┐
  │          │
  │  FALL_   │ ← 判定为跌倒，触发告警
  │  EVENT   │    进入报警模式（内层循环）
  │          │
  └────┬─────┘
       │ GUI 发送 cancel 信号，或手动复位
       ▼
  ┌──────────┐
  │  IDLE    │ ← 回到正常检测模式
  └──────────┘
```

**为什么这样设计？**

- 人摔倒：站立 → 快速下坠（帧差异常大）→ 倒地静止（帧差归零）
- 人走过：持续中等帧差，不会出现"突然变大再归零"的模式
- 风吹帘子/灯光闪烁：帧差小且随机，不会触发状态机
- 单帧判定容易误报，状态机过滤了瞬时干扰

**为什么不依赖 OpenCV？**

- OpenCV 交叉编译到 ARM 平台极其痛苦（依赖十几个库）
- `cv::BackgroundSubtractor` 对 i.MX 6ULL 来说太重
- 帧差法几十行 C 代码就能搞定，代码更精简，更容易交叉编译
- 帧差法本身就是嵌入式安防最常用的基础算法

---

## 7. 硬件要求

| 组件 | 型号/规格 | 说明 |
|------|-----------|------|
| 主控 | i.MX 6ULL (ARM Cortex-A7) | 单核 900MHz，512MB DDR3 |
| 摄像头 | UVC USB 摄像头 | 支持 YUYV 格式，640×480 |
| 显示屏 | LCD 1024×600 | Linux Framebuffer 驱动 |
| 短信模块 | AIR780E (UART) | AT 指令集，UART 通信 |
| LED | GPIO 控制 | sysfs 接口 |
| 网络 | WiFi / 以太网 | 用于 MQTT 推送 |
| 交叉工具链 | `arm-buildroot-linux-gnueabihf-gcc` | Buildroot SDK |

---

## 8. 环境依赖与构建

### 依赖库

| 库 | 用途 | 获取方式 |
|----|------|----------|
| libjpeg (libjpeg-turbo) | JPEG 压缩截图 | 交叉编译放入 `lib/` |
| paho-mqtt3c | MQTT 客户端 | 交叉编译放入 `lib/` |
| pthread | 多线程同步 | 系统自带 |
| librt | 共享内存 | 系统自带 |

### 构建

```bash
cd src

# 交叉编译（目标平台 ARM）
make

# 清理
make clean
```

Makefile 关键配置：
- 编译器：`arm-buildroot-linux-gnueabihf-gcc`
- 编译选项：`-Wall -Wextra`
- NEON 优化：`yuyv_to_rgb_neon.c` 单独使用 `-mfpu=neon -mfloat-abi=hard`
- 链接库：`libjpeg.a`、`libpaho-mqtt3c.a`、`libpthread`、`librt`

---

## 9. 配置说明

复制模板并修改：

```bash
cp config.ini.example config.ini
```

### 完整配置项

```ini
; 摄像头配置
[camera]
device_path = /dev/video1        # V4L2 设备路径
width = 640                      # 采集宽度
height = 480                     # 采集高度
fps = 30                         # 采集帧率
buffer_count = 4                 # 内核缓冲区数量

; 运动检测配置
[motion]
diff_threshold = 70              # 帧差像素阈值 (0-255)
motion_ratio = 0.40              # 运动像素占比阈值 (0.0-1.0)
trigger_frames = 3               # 连续触发帧数

; MQTT 配置
[mqtt]
broker_uri = tcp://localhost:1883  # MQTT Broker 地址
client_id = edgewatcher_001        # 客户端 ID
topic = edgewatcher/alert          # 告警消息主题
keepalive = 60                     # 心跳间隔（秒）

; 短信告警（AIR780E）
[sms]
uart_path = /dev/ttymxc5         # UART 设备路径
phone_number = +8600000000000    # 接收告警短信的手机号

; GPIO
[gpio]
led_pin = 131                    # LED GPIO 引脚编号

; LCD 显示
[lcd]
device_path = /dev/fb0           # Framebuffer 设备
refresh_ms = 200                 # 刷新间隔（毫秒）
```

---

## 10. 运行与测试

### 启动

```bash
# 确保 config.ini 已配置
./fallwatch
```

程序输出示例：
```
V4L2_INIT SUCCESS
MOTION_INIT SUCCESS
GPIO_ALERT_INIT SUCCESS
SMS_ALERT_INIT SUCCESS
MQTT_INIT SUCCESS
LCD_INIT SUCCESS
JPEG_SAVE_INIT SUCCESS
SHM_OUTPUT_INIT SUCCESS
7/7 INIT MODULE SUCCESS
=== ENTERING MAIN LOOP ===
```

按 `Ctrl+C` 正常退出，所有模块自动清理资源。

### 单元测试

每个模块都有独立的测试程序：

| 测试 | 验证内容 |
|------|----------|
| `test_v4l2_capture` | 摄像头采集是否正常 |
| `test_ring_buffer` | 环形缓冲区线程安全 |
| `test_capture_thread` | 采集线程工作正常 |
| `test_motion_detect` | 运动检测算法正确性 |
| `test_mqtt_publisher` | MQTT 连接与发布 |
| `test_sms_alert` | 短信模块 AT 指令 |
| `test_jpeg_save` | JPEG 压缩保存 |

---

## 11. Qt GUI 界面

### 界面布局

```text
┌──────────────────────────────────────────────────┐
│  ┌──────────────────────┐  ┌──────────────────┐  │
│  │                      │  │   ● Watcher      │  │
│  │   摄像头实时预览       │  │   ● PEOPLE       │  │
│  │   (640×480)          │  │   ● Warning      │  │
│  │                      │  │                  │  │
│  │                      │  │   事件日志：       │  │
│  │                      │  │   xx:xx 运动检测  │  │
│  │                      │  │   xx:xx 告警触发  │  │
│  └──────────────────────┘  └──────────────────┘  │
│  状态栏：运行状态 | 帧率 | 连接状态                │
└──────────────────────────────────────────────────┘
```

### 指示灯说明

| 指示灯 | 颜色 | 含义 |
|--------|------|------|
| Watcher | 🟢 绿 / ⚫ 灰 | 采集运行中 / 停止 |
| PEOPLE | 🟡 黄 / ⚫ 灰 | 检测到人（预留）/ 无人 |
| Warning | 🔴 红闪烁 / ⚫ 灰 | 告警中 / 正常 |

### 架构

- **主线程**：UI 事件循环（不阻塞）
- **100ms 定时器**：从共享内存刷新预览 + 检查事件标志位
- **Cancel 按钮**：写入共享内存 cancel 标志 → 主线程退出报警模式
- 所有 UI 控件操作都在 Qt 主线程，不需要额外同步

### 启动

```bash
./scripts/start_gui.sh
```

---

## 12. 设计决策与 FAQ

### Q1：为什么不用 OpenCV？

OpenCV 交叉编译到 ARM 平台极其痛苦（依赖 libstdc++、zlib、libpng 等十几个库），而且 `cv::BackgroundSubtractor` 对 i.MX 6ULL 来说太重。帧差法几十行 C 代码就能搞定，代码更精简，面试官也更认可"自己实现的算法"而非"调个 API"。

### Q2：为什么 YUYV 转 RGB 要手写而不是用 libyuv？

转换逻辑只有几十行代码，引入第三方库反而增加交叉编译复杂度。手写版本可以针对性优化（定点运算、ARM NEON 加速），同时可以在面试时展开讲 BT.601 公式。

### Q3：为什么用互斥锁 + 条件变量，而不用无锁队列？

i.MX 6ULL 是单核 CPU，无锁队列的 CAS 优势发挥不出来。条件变量让消费者在队列空时阻塞等待，不空转烧 CPU。mutex + cond 是最经典、最不容易出 bug 的多线程同步方式。

### Q4：主循环为什么是双层（外层运动检测 + 内层报警模式）？

这是故意设计的。外层正常检测，一旦触发告警进入内层循环：LED 常亮、持续等待 cancel 信号。两层循环天然实现了"正常模式"和"报警模式"的状态隔离，代码逻辑清晰。

### Q5：为什么只检测 Y 分量而不检测 U/V 色度？

Y（亮度）分量已经包含了运动检测需要的全部信息。只处理 Y 分量可以将数据量减半（640×480 = 307K 像素 vs 全帧 614KB），大幅降低 CPU 负载。

### Q6：为什么用 Qt Widgets 而不是 QML？

Widgets 用 C++ 编译，可以直接调 C 接口（共享内存、事件标志位）。QML 需要 QtQuick/QML 引擎，对 i.MX 6ULL 太重。Widgets 编译出的二进制更小，启动更快。

---

本系统仅用于技术研究与学习，检测结果不能替代人工看护。
若出现紧急情况，请第一时间拨打急救电话。
