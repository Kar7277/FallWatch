#include "localdataprovider.h"
#include <QDebug>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

// 和 C 后端 shm_output.h 保持一致的结构体定义
struct ShmFrame {
    volatile int frame_id;
    volatile int ready;       // 1=C正在写, 0=QT可以读
    long long timestamp;
    unsigned char rgb[640 * 480 * 3];  // RGB24
};

struct ShmEvents {
    volatile int watcher;     // 0=停, 1=运行
    volatile int motion;      // 0=无运动, 1=有运动
    volatile int warning;     // 0=正常, 1=报警中
    volatile int cancel;      // QT→C: 1=取消报警
};

#define SHM_FRAME_NAME  "/fallwatch_frame"
#define SHM_EVENTS_NAME "/fallwatch_events"

LocalDataProvider::LocalDataProvider()
{
}

LocalDataProvider::~LocalDataProvider()
{
    disconnect();
}

bool LocalDataProvider::connect()
{
    // 打开帧共享内存
    m_shmFd_frame = shm_open(SHM_FRAME_NAME, O_RDWR, 0666);
    if (m_shmFd_frame < 0) {
        m_shmFd_frame = shm_open(SHM_FRAME_NAME, O_RDONLY, 0666);
        if (m_shmFd_frame < 0) {
            qCritical("LocalDataProvider: cannot open %s", SHM_FRAME_NAME);
            return false;
        }
    }
    m_shmFrame = mmap(NULL, sizeof(ShmFrame),
                      PROT_READ, MAP_SHARED,
                      m_shmFd_frame, 0);
    if (m_shmFrame == MAP_FAILED) {
        qCritical("LocalDataProvider: mmap(frame) failed");
        close(m_shmFd_frame);
        m_shmFd_frame = -1;
        return false;
    }

    // 打开事件共享内存
    m_shmFd_events = shm_open(SHM_EVENTS_NAME, O_RDWR, 0666);
    if (m_shmFd_events < 0) {
        qCritical("LocalDataProvider: cannot open %s", SHM_EVENTS_NAME);
        munmap(m_shmFrame, sizeof(ShmFrame));
        m_shmFrame = nullptr;
        close(m_shmFd_frame);
        m_shmFd_frame = -1;
        return false;
    }
    m_shmEvents = mmap(NULL, sizeof(ShmEvents),
                       PROT_READ | PROT_WRITE, MAP_SHARED,
                       m_shmFd_events, 0);
    if (m_shmEvents == MAP_FAILED) {
        qCritical("LocalDataProvider: mmap(events) failed");
        close(m_shmFd_events);
        m_shmFd_events = -1;
        munmap(m_shmFrame, sizeof(ShmFrame));
        m_shmFrame = nullptr;
        close(m_shmFd_frame);
        m_shmFd_frame = -1;
        return false;
    }

    qDebug("LocalDataProvider: connected via shared memory");
    return true;
}

void LocalDataProvider::disconnect()
{
    if (m_shmEvents) {
        munmap(m_shmEvents, sizeof(ShmEvents));
        m_shmEvents = nullptr;
    }
    if (m_shmFd_events >= 0) {
        close(m_shmFd_events);
        m_shmFd_events = -1;
    }
    if (m_shmFrame) {
        munmap(m_shmFrame, sizeof(ShmFrame));
        m_shmFrame = nullptr;
    }
    if (m_shmFd_frame >= 0) {
        close(m_shmFd_frame);
        m_shmFd_frame = -1;
    }
}

QImage LocalDataProvider::getPreviewFrame()
{
    if (!m_shmFrame) return QImage();

    ShmFrame *f = (ShmFrame*)m_shmFrame;

    // C 端正在写，跳过这帧
    if (f->ready != 0) return QImage();

    QImage img(f->rgb, 640, 480, 640 * 3, QImage::Format_RGB888);

    QMutexLocker lock(&m_frameMutex);
    // deep copy：共享内存随时被覆盖
    m_lastFrame = img.copy();
    return m_lastFrame;
}

int LocalDataProvider::getWatcherStatus()
{
    if (!m_shmEvents) return 0;
    ShmEvents *e = (ShmEvents*)m_shmEvents;
    return e->watcher;
}

int LocalDataProvider::getMotionStatus()
{
    if (!m_shmEvents) return 0;
    ShmEvents *e = (ShmEvents*)m_shmEvents;
    int val = e->motion;
    // 只在上升沿（0→1）追加一次日志
    if (val && !m_lastMotion) {
        QMutexLocker lock(&m_eventMutex);
        m_events.append("检测到运动");
    }
    m_lastMotion = val;
    return val;
}

int LocalDataProvider::getWarningStatus()
{
    if (!m_shmEvents) return 0;
    ShmEvents *e = (ShmEvents*)m_shmEvents;
    int val = e->warning;
    // 上升沿 0→1：警报触发
    if (val && !m_lastWarning) {
        QMutexLocker lock(&m_eventMutex);
        m_events.append("警报已触发");
    }
    // 下降沿 1→0：警报已取消
    if (!val && m_lastWarning) {
        QMutexLocker lock(&m_eventMutex);
        m_events.append("警报已取消");
    }
    m_lastWarning = val;
    return val;
}

QStringList LocalDataProvider::getNewEvents()
{
    QMutexLocker lock(&m_eventMutex);
    QStringList events = m_events;
    m_events.clear();
    return events;
}

void LocalDataProvider::cancelAlarm()
{
    if (!m_shmEvents) return;
    ShmEvents *e = (ShmEvents*)m_shmEvents;
    e->cancel = 1;
    qDebug("LocalDataProvider: cancelAlarm() sent via shm");
}
