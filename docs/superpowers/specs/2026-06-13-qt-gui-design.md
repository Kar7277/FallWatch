# EdgeWatcher QT 界面设计

## 概述

在已完成 C 后端（V4L2 采集、RingBuffer、运动检测、告警、LCD、MQTT、JPEG）基础上，开发 QT Widgets 上位机界面，同时支持**板子本地运行**和 **PC 远程监看**两种模式。

---

## 一、核心冲突与解决方案

### 冲突
- `lcd_display.c` 直接写 `/dev/fb0` 做摄像头实时预览
- QT linuxfb 插件也需要独占 `/dev/fb0`
- 两者无法同时使用同一 framebuffer

### 解决
| 模式 | fb0 归属 | 摄像头画面显示方式 |
|------|----------|-------------------|
| A 板子版 | **QT 独占** | C 后端通过共享内存传 RGB 帧给 QT |
| B PC 版 | C 后端 lcd_display | PC 端 QT 通过 TCP 接收 JPEG 帧 |

---

## 二、整体架构

```
┌─────────────────────────────────────────┐
│              QT UI (C++/Widgets)          │
│   预览区 │ 状态灯 │ 日志 │ 取消报警按钮    │
└────────────────┬────────────────────────┘
                 │ IEdgeWatcherData (抽象接口)
      ┌──────────┴──────────┐
      ▼                     ▼
┌──────────────┐    ┌──────────────┐
│ LocalData     │    │ RemoteData    │
│ 共享内存       │    │ TCP + MQTT    │
│ (板子版)      │    │ (PC版)        │
└──────┬───────┘    └──────┬───────┘
       │                   │
┌──────┴───────────────────┴──────────┐
│           C 后端 (已有)              │
│  采集 → RingBuffer → 检测 → 告警     │
│  [新增] shm_output │ tcp_stream     │
└─────────────────────────────────────┘
```

---

## 三、QT 界面布局

```
┌──────────────────────────────────────────────┐
│  ┌────────────────────┐  ┌─────────────────┐ │
│  │                    │  │ ● 运行状态       │ │
│  │                    │  │ ● 运动检测       │ │
│  │   摄像头实时预览     │  │ ● 告警状态       │ │
│  │   640 × 480        │  │                 │ │
│  │                    │  │ [取消报警] 按钮   │ │
│  └────────────────────┘  └─────────────────┘ │
│  ┌──────────────────────────────────────────┐ │
│  │ 日志列表 (QTextEdit 只读)                  │ │
│  └──────────────────────────────────────────┘ │
│  ── QStatusBar ───────────────────────────── │
└──────────────────────────────────────────────┘
```

### 控件清单

| 控件 | 类型 | 说明 |
|------|------|------|
| 预览区 | QLabel + QImage | 100ms 定时刷新 RGB 数据 |
| Watcher 灯 | 自定义 StatusDot | 绿=采集运行中 / 灰=停止 |
| Motion 灯 | 自定义 StatusDot | 黄=检测到运动 / 灰=无运动 |
| Warning 灯 | 自定义 StatusDot | 红闪烁=告警中 / 灰=正常 |
| 取消报警 | QPushButton | 报警后手动复位到空闲状态 |
| 日志区 | QTextEdit | 只读，自动滚动最新事件 |
| 状态栏 | QStatusBar | 帧率 / 连接模式 / 网络状态 |

---

## 四、数据层抽象

```cpp
// 抽象接口：板子版和 PC 版各自实现
class IDataProvider {
public:
    virtual ~IDataProvider() {}
    virtual QImage getPreviewFrame() = 0;     // 拿最新帧
    virtual int    getWatcherStatus() = 0;    // 0=停 1=运行
    virtual int    getMotionStatus() = 0;     // 0=无 1=有
    virtual int    getWarningStatus() = 0;    // 0=正常 1=报警
    virtual QStringList getNewEvents() = 0;   // 新日志
    virtual void   cancelAlarm() = 0;         // 取消报警
};
```

### LocalDataProvider（A 板子版）
- 帧来源：读共享内存（shm_frame）
- 事件来源：读共享内存（shm_events）
- cancelAlarm：写共享内存中的 cancel 标志

### RemoteDataProvider（B PC 版）
- 帧来源：TCP 接收 JPEG → QImage 解码
- 事件来源：MQTT 消息回调（复用已有 mqtt_publisher 主题）
- cancelAlarm：发 MQTT 控制消息到板子

---

## 五、C 后端新增模块

### 5.1 共享内存输出 `shm_output.c/h`

给 A 板子版用，替代 `lcd_display` 功能。

```c
// 共享内存结构
struct shm_frame {
    volatile int frame_id;
    volatile int ready;       // 0=QT可读 1=C在写
    long long timestamp;
    unsigned char rgb[640 * 480 * 3];  // RGB24
};

struct shm_events {
    volatile int watcher;     // 0/1
    volatile int motion;      // 0/1
    volatile int warning;     // 0/1
    volatile int cancel;      // QT写入, C读取
};
```

接口：
- `shm_output_init()` — 创建共享内存
- `shm_output_write_frame()` — 每帧写 RGB 数据
- `shm_output_write_events()` — 更新事件状态
- `shm_output_read_cancel()` — 读取取消标志
- `shm_output_destroy()` — 清理

### 5.2 TCP 帧流服务 `tcp_stream.c/h`

给 B PC 版用，将 JPEG 帧发送到 PC。

接口：
- `tcp_stream_init(port)` — 监听端口
- `tcp_stream_send_frame(jpeg_data, len)` — 发送压缩帧
- `tcp_stream_destroy()` — 清理

利用已有 `yuyv_to_rgb_row` + libjpeg 压缩后用 `send()` 发送。协议很简单：4 字节长度（网络字节序）+ JPEG 数据。

---

## 六、两种模式对比

| | A 板子版 | B PC 版 |
|---|---|---|
| C 后端在哪 | 板子 | 板子 |
| QT 在哪 | 板子（交叉编译） | PC（本地编译） |
| fb0 归属 | QT 独占 | C 后端 lcd_display |
| lcd_display | **关闭** | 保留（板子 LCD 可见） |
| 帧传输 | 共享内存 RGB | TCP JPEG |
| 事件传输 | 共享内存 | MQTT |
| QT 编译 | arm-buildroot-qmake | g++ qmake |
| 开发调试 | 不方便 | IDE 直接断点 |

---

## 七、开发顺序

```
① B 版 PC 端先开发
   ├── QT 项目骨架 + CMakeLists/qmake
   ├── RemoteDataProvider (MQTT + TCP)
   ├── 所有 UI 控件（预览/指示灯/日志/取消按钮）
   └── 本地 PC 调通 → 能看到摄像头帧

② 交叉编译 A 版
   ├── 交叉编译 Qt 5.x 库
   ├── 编译板子版 QT 可执行文件
   └── 部署到板子验证

③ C 后端加共享内存
   ├── shm_output.c
   └── main.c 集成（A 模式下关 lcd_display，开 shm_output）

④ C 后端加 TCP 流服务
   ├── tcp_stream.c
   └── main.c 集成

⑤ 两端联调
   ├── A 版：板子 QT + 共享内存 → 全功能验证
   └── B 版：PC QT + TCP/MQTT → 远程监看验证
```

---

## 八、关键定时器

| 定时器 | 间隔 | 任务 |
|--------|------|------|
| 预览刷新 | 100ms | 读帧数据 → 更新预览区 QImage |
| 状态刷新 | 200ms | 读事件 → 更新三个 StatusDot |
| 日志刷新 | 500ms | 读日志队列 → 追加 QTextEdit |
| 帧率计算 | 1000ms | 统计帧数 → 更新状态栏 |

---

## 九、取消报警流程

```
QT [取消报警] 按钮点击
    ↓
A版: 写共享内存 cancel=1 → C 后端检测到 → 停止告警 → warning=0
B版: 发 MQTT "edgewatcher/cmd" → C 后端收到 → 同上
    ↓
QT 检测到 warning=0 → 红灯灭 → 恢复监控
```

---

## 十、待确定（可后续细化）

1. PC 版 TCP 帧的码率/帧率控制（每帧都发还是降采样）
2. 是否后续加入人物识别的 PEOPLES 灯（原设计预留）
3. QT 编译方式选 qmake 还是 CMake（qmake 对 Qt5 原生支持更好）

---

**文档版本:** v1.0 | **日期:** 2026-06-13
