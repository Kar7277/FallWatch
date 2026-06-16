# FallWatch — 边缘计算跌倒检测系统

基于 ARM Linux 嵌入式平台的智能跌倒检测系统，使用摄像头实时采集视频，通过运动检测算法识别跌倒事件，并触发多重告警机制（MQTT、短信、GPIO LED）。

## 功能

- **视频采集**：V4L2 摄像头驱动，支持 YUYV 格式
- **运动检测**：基于帧差分的运动检测算法
- **多重告警**：MQTT 推送、SIM800 短信、GPIO LED 闪烁
- **本地显示**：Framebuffer LCD 实时画面显示
- **JPEG 截图**：触发时保存现场截图
- **Qt GUI**：桌面端监控界面（可选）
- **NEON 加速**：ARM NEON SIMD 优化 YUYV→RGB 转换

## 硬件要求

- ARM Linux 开发板（如 i.MX6ULL）
- USB 摄像头（UVC）
- SIM800 短信模块（UART）
- GPIO LED
- LCD 屏幕（Framebuffer）

## 构建

```bash
cd src
make
```

## 配置

复制配置模板并修改：

```bash
cp config.ini.example config.ini
# 编辑 config.ini 填入实际参数
```

## 运行

```bash
./fallwatch
```

GUI 监控界面：

```bash
./scripts/start_gui.sh
```

## 项目结构

```
FallWatch/
├── src/          # 主程序源码
├── inc/          # 头文件
├── gui/          # Qt GUI 监控界面
├── lib/          # 第三方库
├── test/         # 单元测试
├── tools/        # 工具程序
├── scripts/      # 启动脚本
└── docs/         # 文档和设计笔记
```

## 许可证

MIT License
