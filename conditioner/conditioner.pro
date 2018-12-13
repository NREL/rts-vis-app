QT       += core network concurrent websockets

TARGET = conditioner
CONFIG += app

macx {
    CONFIG -= app_bundle
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

TEMPLATE = app

QMAKE_CXXFLAGS += -march=native 

CONFIG += c++14


SOURCES += \
    main.cpp \
    zmqworker.cpp \
    session.cpp \
    datacontrol.cpp \
    samplebuffer.cpp

HEADERS += \
    zmqworker.h \
    session.h \
    datacontrol.h \
    samplebuffer.h \
    ext/zmq.hpp

DISTFILES += \
    README.md \
    LICENSE

