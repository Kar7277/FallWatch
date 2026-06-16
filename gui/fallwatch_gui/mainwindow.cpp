#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(IDataProvider *provider, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_provider(provider)
{
    ui->setupUi(this);

    setWindowTitle("FallWatch - 智能跌倒监控系统");

    // 用真正的 StatusDot 替换 UI 里三个 ● 占位 QLabel
    replaceDotLabels();

    // 取消报警按钮
    connect(ui->cancelBtn, &QPushButton::clicked,
            this, &MainWindow::onCancelAlarm);

    // 预览刷新 100ms = 10fps
    m_previewTimer = new QTimer(this);
    connect(m_previewTimer, &QTimer::timeout,
            this, &MainWindow::refreshPreview);
    m_previewTimer->start(100);

    // 状态灯刷新 200ms
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout,
            this, &MainWindow::refreshStatus);
    m_statusTimer->start(200);

    // 日志刷新 500ms
    m_logTimer = new QTimer(this);
    connect(m_logTimer, &QTimer::timeout,
            this, &MainWindow::refreshLog);
    m_logTimer->start(500);

    ui->logView->append(QString("[%1] 系统启动")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}

MainWindow::~MainWindow()
{
    if (m_provider) {
        m_provider->disconnect();
        delete m_provider;
    }
    delete ui;
}

// 遍历嵌套布局，找到包含指定 widget 的 QHBoxLayout
// 因为 .ui 里 statusGroup 的布局结构是：
//   verticalLayout_5 → verticalLayout_4 → {hLayout_8, hLayout_5, hLayout_7}
// 直接 parentWidget()->layout() 拿到的外层 QVBoxLayout，不是内层 QHBoxLayout
static QHBoxLayout *findRowLayout(QLayout *top, QWidget *target)
{
    for (int i = 0; i < top->count(); i++) {
        QLayoutItem *item = top->itemAt(i);
        if (!item) continue;
        if (item->widget() == target) {
            // 当前层就是 Horizontal 层
            return qobject_cast<QHBoxLayout*>(top);
        }
        if (item->layout()) {
            QHBoxLayout *found = findRowLayout(item->layout(), target);
            if (found) return found;
        }
    }
    return nullptr;
}

// 把 UI 文件里三个 ● QLabel 拿出来，换成 StatusDot
void MainWindow::replaceDotLabels()
{
    QLayout *top = ui->statusGroup->layout();
    if (!top) return;

    // --- Watcher 灯 ---
    QHBoxLayout *rowW = findRowLayout(top, ui->dotWatcher);
    if (rowW) {
        int idx = rowW->indexOf(ui->dotWatcher);
        ui->dotWatcher->hide();
        rowW->removeWidget(ui->dotWatcher);
        m_watcherDot = new StatusDot(this);
        m_watcherDot->setOnColor(QColor("#00CC66"));
        rowW->insertWidget(idx, m_watcherDot);
    }

    // --- Motion 灯 ---
    QHBoxLayout *rowM = findRowLayout(top, ui->label_2);
    if (rowM) {
        int idx = rowM->indexOf(ui->label_2);
        ui->label_2->hide();
        rowM->removeWidget(ui->label_2);
        m_motionDot = new StatusDot(this);
        m_motionDot->setOnColor(QColor("#FFCC00"));
        rowM->insertWidget(idx, m_motionDot);
    }

    // --- Warning 灯 ---
    QHBoxLayout *rowA = findRowLayout(top, ui->label_3);
    if (rowA) {
        int idx = rowA->indexOf(ui->label_3);
        ui->label_3->hide();
        rowA->removeWidget(ui->label_3);
        m_warningDot = new StatusDot(this);
        m_warningDot->setOnColor(QColor("#FF3333"));
        rowA->insertWidget(idx, m_warningDot);
    }
}

void MainWindow::refreshPreview()
{
    QImage frame = m_provider->getPreviewFrame();
    if (!frame.isNull()) {
        ui->previewLabel->setPixmap(QPixmap::fromImage(frame));
    }
}

void MainWindow::refreshStatus()
{
    int w = m_provider->getWatcherStatus();
    m_watcherDot->setState(w ? StatusDot::On : StatusDot::Off);

    int m = m_provider->getMotionStatus();
    m_motionDot->setState(m ? StatusDot::On : StatusDot::Off);

    int a = m_provider->getWarningStatus();
    m_warningDot->setState(a ? StatusDot::Blinking : StatusDot::Off);

    ui->cancelBtn->setEnabled(a != 0);
}

void MainWindow::refreshLog()
{
    QStringList events = m_provider->getNewEvents();
    for (const QString &ev : events) {
        ui->logView->append(QString("[%1] %2")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(ev));
    }
}

void MainWindow::onCancelAlarm()
{
    m_provider->cancelAlarm();
    ui->logView->append(QString("[%1] 用户手动取消报警")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
}
