#!/bin/sh
# FallWatch QT GUI 启动脚本
# 必须设置 tslib 通用插件 + linuxfb 平台，否则触摸输入无效

# 禁止 LCD 休眠
echo -e "\033[9;0]" > /dev/tty0

# 设置 QT 环境
export QT_QPA_GENERIC_PLUGINS=tslib:/dev/input/event1
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
export QT_QPA_FONTDIR=/usr/lib/fonts/

# 先启动 C 后端（如果还没跑的话）
if ! pidof fallwatch > /dev/null 2>&1; then
    echo "Starting C backend..."
    ./fallwatch &
    sleep 3
fi

# 启动 QT GUI
echo "Starting QT GUI..."
./fallwatch_gui --local
