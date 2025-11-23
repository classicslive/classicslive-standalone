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
  cls_hook_dolphin.cpp \
  cls_hook_infuse.cpp \
  cls_hook_kemulator.cpp \
  cls_hook_ryujinx.cpp \
  cls_hook_touchhle.cpp \
  cls_hook_xemu.cpp \
  cls_hook_yuzu.cpp \
  cls_process_select.cpp \
  cl_frontend.cpp \
  cls_hook.cpp \
  cls_hook_cemu.cpp \
  cls_network_manager.cpp \
  cls_thread.cpp

HEADERS += \
  cls_hook.h \
  cls_hook_cemu.h \
  cls_hook_dolphin.h \
  cls_hook_infuse.h \
  cls_hook_kemulator.h \
  cls_hook_ryujinx.h \
  cls_hook_touchhle.h \
  cls_hook_xemu.h \
  cls_hook_yuzu.h \
  cls_identification_methods.h \
  cls_main.h \
  cls_network_manager.h \
  cls_process_select.h \
  cls_thread.h
