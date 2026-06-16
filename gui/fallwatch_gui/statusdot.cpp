#include "statusdot.h"
  #include <QPainter>   // 画图需要

  // 构造函数
  StatusDot::StatusDot(QWidget *parent)
      : QWidget(parent)
  {
      setFixedSize(24, 24);  // 固定大小 24×24 像素

      // 闪烁定时器：每 500ms 翻转一次 m_lit，然后重绘
      connect(&m_blinkTimer, SIGNAL(timeout()), this, SLOT(onBlinkTimer()));
  }

  // 设置亮时的颜色
  void StatusDot::setOnColor(const QColor &color)
  {
      m_onColor = color;
      update();  // 立即重绘
  }

  // 设置灭时的颜色
  void StatusDot::setOffColor(const QColor &color)
  {
      m_offColor = color;
      update();
  }

  // 切换状态
  void StatusDot::setState(State state)
  {
      m_state = state;

      if (state == Blinking) {
          m_blinkTimer.setInterval(500);  // 设间隔
          m_blinkTimer.start();           // 启动
      } else {
          m_blinkTimer.stop();      // 停止定时器
          m_lit = (state == On);    // On→亮, Off→灭
      }

      update();  // 立即重绘
  }

  // 获取当前状态
  StatusDot::State StatusDot::state() const
  {
      return m_state;
  }

  // 绘制事件：用 QPainter 画一个填充椭圆
  void StatusDot::paintEvent(QPaintEvent *)
  {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing, true);  // 抗锯齿，圆更光滑

      // 判断当前该画什么颜色
      bool drawLit = false;
      switch (m_state) {
      case On:       drawLit = true;  break;
      case Off:      drawLit = false; break;
      case Blinking: drawLit = m_lit; break;  // 闪烁时看当前相位
      }

      QColor color = drawLit ? m_onColor : m_offColor;

      painter.setBrush(color);       // 填充颜色
      painter.setPen(Qt::NoPen);     // 无边框
      painter.drawEllipse(rect().adjusted(2, 2, -2, -2));  // 缩 2px 画圆
  }
  void StatusDot::onBlinkTimer()
  {
        m_lit = !m_lit;   // 翻转亮灭
        update();         // 触发重绘
  }
