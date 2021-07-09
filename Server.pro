#-------------------------------------------------
#
# Project created by QtCreator 2016-10-31T07:02:16
#
#-------------------------------------------------

QT       += core gui network sql serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Server
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    csmodbusserver.cpp \
    cssettings.cpp \
    cssqlitestorage.cpp \
    cssupervisorserver.cpp \
    server.cpp \
    csfatekcollector.cpp \
    bore.cpp \
    smsform.cpp \
    smsnotifications.cpp \
	3rdParty/SimpleQtLogger/simpleQtLogger.cpp \
    csmodbuscollector.cpp

HEADERS  += mainwindow.h \
    cstypes.h \
    csmodbusserver.h \
    cssettings.h \
    cssqlitestorage.h \
    cssupervisorserver.h \
    server.h \
    csfatekcollector.h \
    bore.h \
    smsform.h \
    smsnotifications.h \
	3rdParty/SimpleQtLogger/simpleQtLogger.h \
    csmodbuscollector.h

FORMS    += mainwindow.ui \
    bore.ui \
    smsform.ui
