QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    statusdot.cpp \
    localdataprovider.cpp

HEADERS += \
    mainwindow.h \
    statusdot.h \
    idataprovider.h \
    localdataprovider.h

FORMS += \
    mainwindow.ui

# shm_open 需要 librt
unix: LIBS += -lrt

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
