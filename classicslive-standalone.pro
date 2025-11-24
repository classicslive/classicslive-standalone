QT += core gui network multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

win32 {
  LIBS += -LPsapi.lib -lpsapi
}

GIT_VERSION = $$system(git rev-parse --short=8 HEAD 2>$$QMAKE_NULL_DEVICE)
isEmpty(GIT_VERSION): GIT_VERSION = unknown

DEFINES += \
  GIT_VERSION=\\\"$$GIT_VERSION\\\" \
  CL_EXTERNAL_MEMORY=1 \
  CL_HAVE_EDITOR=1 \
  CL_HAVE_FILESYSTEM=1

include(classicslive-integration/classicslive-integration.pri)

SOURCES += \
  cls_process_select.cpp \
  cl_frontend.cpp \
  cls_hook.cpp \
  cls_network_manager.cpp \
  cls_thread.cpp \
  hooks/cemu.cpp \
  hooks/dolphin.cpp \
  hooks/infuse.cpp \
  hooks/kemulator.cpp \
  hooks/ryujinx.cpp \
  hooks/touchhle.cpp \
  hooks/xemu.cpp \
  hooks/yuzu.cpp

HEADERS += \
  cls_hook.h \
  cls_main.h \
  cls_network_manager.h \
  cls_process_select.h \
  cls_thread.h \
  hooks/cemu.h \
  hooks/dolphin.h \
  hooks/infuse.h \
  hooks/kemulator.h \
  hooks/ryujinx.h \
  hooks/touchhle.h \
  hooks/xemu.h \
  hooks/yuzu.h
