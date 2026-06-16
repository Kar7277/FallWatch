#ifndef IDATAPROVIDER_H
#define IDATAPROVIDER_H

#include <QImage>
#include <QStringList>

class IDataProvider
{
public:
    virtual ~IDataProvider() {}

    virtual bool connect() = 0;
    virtual void disconnect() = 0;

    virtual QImage getPreviewFrame() = 0;

    virtual int getWatcherStatus() = 0;
    virtual int getMotionStatus() = 0;
    virtual int getWarningStatus() = 0;

    virtual QStringList getNewEvents() = 0;

    virtual void cancelAlarm() = 0;
};

#endif // IDATAPROVIDER_H
