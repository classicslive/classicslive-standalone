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
  CL_HAVE_FILESYSTEM=1 \
  CL_HAVE_SSL=0

include(classicslive-integration/classicslive-integration.pri)

linux {
  # Grants CAP_SYS_PTRACE through pkexec where Yama blocks reading game memory.
  # Install with `make install`; override the location with qmake PREFIX=...
  isEmpty(PREFIX): PREFIX = /usr/local
  CLS_HELPER_PATH = $$PREFIX/libexec/classicslive/classicslive-ptrace-helper

  DEFINES += \
    CLS_HELPER_PATH=\\\"$$CLS_HELPER_PATH\\\" \
    CLS_HELPER_SOURCE_PATH=\\\"$$PWD/tools/classicslive-ptrace-helper\\\"

  # Point the polkit policy at the installed helper
  CLS_POLICY = $$cat($$PWD/tools/org.classicslive.ptrace-helper.policy.in, blob)
  CLS_POLICY = $$replace(CLS_POLICY, @CLS_HELPER_PATH@, $$CLS_HELPER_PATH)
  write_file($$OUT_PWD/tools/org.classicslive.ptrace-helper.policy, CLS_POLICY)|error()

  target.path = $$PREFIX/bin
  helper.path = $$PREFIX/libexec/classicslive
  helper.files = tools/classicslive-ptrace-helper
  policy.path = /usr/share/polkit-1/actions
  policy.files = $$OUT_PWD/tools/org.classicslive.ptrace-helper.policy
  policy.CONFIG += no_check_exist
  INSTALLS += target helper policy
}

SOURCES += \
  cls_login_dialog.cpp \
  cls_permissions.cpp \
  cls_process_select.cpp \
  cl_frontend.cpp \
  cls_hook.cpp \
  cls_network_manager.cpp \
  cls_thread.cpp \
  hooks/cemu.cpp \
  hooks/citra.cpp \
  hooks/dolphin.cpp \
  hooks/infuse.cpp \
  hooks/kemulator.cpp \
  hooks/ryujinx.cpp \
  hooks/shockwave.cpp \
  hooks/touchhle.cpp \
  hooks/vita3k.cpp \
  hooks/xemu.cpp \
  hooks/xenia.cpp \
  hooks/yuzu.cpp

HEADERS += \
  cls_hook.h \
  cls_login_dialog.h \
  cls_main.h \
  cls_permissions.h \
  cls_network_manager.h \
  cls_process_select.h \
  cls_thread.h \
  hooks/cemu.h \
  hooks/citra.h \
  hooks/dolphin.h \
  hooks/infuse.h \
  hooks/kemulator.h \
  hooks/horizon.h \
  hooks/ryujinx.h \
  hooks/shockwave.h \
  hooks/touchhle.h \
  hooks/vita3k.h \
  hooks/xemu.h \
  hooks/xenia.h \
  hooks/yuzu.h
