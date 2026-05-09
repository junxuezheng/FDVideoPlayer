#-------------------------------------------------
#
# Project created by QtCreator 2026-03-20T17:45:18
#
#-------------------------------------------------

QT += core widgets gui multimedia   # 必须包含 multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = FDVideoPlayer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

RC_ICONS = $$PWD/picture/wz3.ico

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    video_thread.cpp \
    video_widget.cpp

HEADERS += \
        mainwindow.h \
    video_widget.h \
    video_thread.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/ffmpeg-8.0.1-full_build-shared/lib/ \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavutil \
        -lswresample \
        -lswscale
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/ffmpeg-8.0.1-full_build-shared/lib/ \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavutil \
        -lswresample \
        -lswscale

INCLUDEPATH += $$PWD/ffmpeg-8.0.1-full_build-shared/include
DEPENDPATH += $$PWD/ffmpeg-8.0.1-full_build-shared/include



LIBS += -L$$PWD/SDL3/lib/ -lSDL3

INCLUDEPATH += $$PWD/SDL3/include
DEPENDPATH += $$PWD/SDL3/include
