#include "mainwindow.h"
#include "localdataprovider.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LocalDataProvider *provider = new LocalDataProvider();
    if (!provider->connect()) {
        qCritical("无法连接到共享内存，请确认 FallWatch C 进程已启动");
        delete provider;
        return 1;
    }

    MainWindow w(provider);
    w.show();

    return a.exec();
}
