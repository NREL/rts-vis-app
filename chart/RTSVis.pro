QT += gui widgets

CONFIG += c++14

macx {
    INCLUDEPATH += /usr/local/include
    LIBS += /usr/local/lib/libzmq.a
    LIBS += -framework Foundation
}

linux {
    LIBS += -lzmq
}

win32 {
    INCLUDEPATH += /opt/glm/0.9.8.5/include
    DEFINES += ZMQ_STATIC
    LIBS += -lzmq -lws2_32 -lsodium -liphlpapi
}

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
        main.cpp \
    chartmaster.cpp \
    chartwidget.cpp \
    chartdata.cpp \
    chart.cpp \
    comm/datacontrol.cpp \
    comm/samplebuffer.cpp \
    comm/session.cpp \
    comm/zmqworker.cpp \
    flowlayout.cpp \
    startupdialog.cpp \
    tooldialog.cpp \
    verticallabel.cpp \
    common.cpp

RESOURCES +=

FORMS += \
    chartmaster.ui \
    startupdialog.ui \
    tooldialog.ui

HEADERS += \
    chartmaster.h \
    chartwidget.h \
    chartdata.h \
    chart.h \
    comm/datacontrol.h \
    comm/samplebuffer.h \
    comm/session.h \
    comm/zmqworker.h \
    flowlayout.h \
    startupdialog.h \
    ext/zmq.hpp \
    tooldialog.h \
    verticallabel.h \
    common.h

DISTFILES += \
    README.md
