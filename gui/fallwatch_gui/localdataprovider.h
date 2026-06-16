#ifndef LOCALDATAPROVIDER_H
#define LOCALDATAPROVIDER_H

#include "idataprovider.h"
#include <QImage>
#include <QStringList>
#include <QMutex>

class LocalDataProvider : public IDataProvider
{
public:
    LocalDataProvider();
    ~LocalDataProvider() override;

    bool connect() override;
    void disconnect() override;

    QImage getPreviewFrame() override;
    int getWatcherStatus() override;
    int getMotionStatus() override;
    int getWarningStatus() override;
    QStringList getNewEvents() override;
    void cancelAlarm() override;

private:
    int m_shmFd_frame = -1;
    int m_shmFd_events = -1;
    void *m_shmFrame = nullptr;
    void *m_shmEvents = nullptr;

    QMutex m_frameMutex;
    QImage m_lastFrame;

    QStringList m_events;
    QMutex m_eventMutex;

    int m_lastMotion = 0;
    int m_lastWarning = 0;
};

#endif // LOCALDATAPROVIDER_H
