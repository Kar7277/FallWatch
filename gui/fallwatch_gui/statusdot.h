#ifndef STATUSDOT_H
  #define STATUSDOT_H

  #include <QWidget>
  #include <QColor>
  #include <QTimer>

  // 自定义状态指示灯：圆点，支持 灭/亮/闪烁 三种状态
  class StatusDot : public QWidget
  {
      Q_OBJECT  // Qt 元对象系统必须，启用信号槽机制

  public:
      // 状态枚举
      enum State {
          Off,      // 灰色灭
          On,       // 常亮
          Blinking  // 闪烁（500ms 交替亮灭）
      };

      // 构造函数：parent 是父窗口
      explicit StatusDot(QWidget *parent = nullptr);

      // 设置亮色（默认为绿色）
      void setOnColor(const QColor &color);

      // 设置灭色（默认暗灰）
      void setOffColor(const QColor &color);

      // 切换状态
      void setState(State state);

      // 获取当前状态
      State state() const;

  protected:
      // 重写绘制事件——每次 update() 调用时触发
      void paintEvent(QPaintEvent *event) override;

  private:
      QColor m_onColor  = QColor("#00FF00");  // 亮时的颜色，默认绿色
      QColor m_offColor = QColor("#404040");  // 灭时的颜色，默认暗灰
      State  m_state    = Off;                // 当前状态
      bool   m_lit      = false;             // true=当前亮, false=当前灭（闪烁用）
      QTimer m_blinkTimer;                   // 闪烁定时器，500ms 翻转一次
  private slots:
      void onBlinkTimer(); // 闪烁切换槽函数
  };

  #endif // STATUSDOT_H
