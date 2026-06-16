#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QTimer>
#include <QStatusBar>
#include "idataprovider.h"
#include "statusdot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(IDataProvider *provider, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshPreview();
    void refreshStatus();
    void refreshLog();
    void onCancelAlarm();

private:
    void replaceDotLabels();  // 用 StatusDot 替换 UI 里的 ● QLabel

    Ui::MainWindow *ui;
    IDataProvider *m_provider;

    StatusDot *m_watcherDot = nullptr;
    StatusDot *m_motionDot = nullptr;
    StatusDot *m_warningDot = nullptr;

    QTimer *m_previewTimer;
    QTimer *m_statusTimer;
    QTimer *m_logTimer;
};

#endif // MAINWINDOW_H
