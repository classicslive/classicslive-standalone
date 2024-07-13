QT += core gui network multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

LIBS += -LPsapi.lib -lpsapi

GIT_VERSION = $$system(git describe --tags)

DEFINES += \
  GIT_VERSION=\\\"$$GIT_VERSION\\\" \
  HAVE_CHD \
  HAVE_STRL \
  WANT_SUBCODE \
  WANT_RAW_DATA_SECTOR \
  CL_EXTERNAL_MEMORY=1 \
  CL_HAVE_EDITOR=1

QMAKE_CFLAGS += -std=c89

INCLUDEPATH += \
  $$PWD/classicslive-integration \
  $$PWD/libretro-common/include \
  $$PWD/libretro-common/include/libchdr \
  $$PWD/libretro-common/include/vfs

SOURCES += \
  cls_hook_infuse.cpp \
  cls_hook_ryujinx.cpp \
  cls_hook_touchhle.cpp \
  libretro-common/compat/compat_strl.c \
  libretro-common/compat/fopen_utf8.c \
  libretro-common/encodings/encoding_base64.c \
  libretro-common/encodings/encoding_crc32.c \
  libretro-common/encodings/encoding_utf.c \
  libretro-common/file/file_path.c \
  libretro-common/formats/json/jsonsax.c \
  libretro-common/formats/json/jsonsax_full.c \
  libretro-common/formats/libchdr/libchdr_bitstream.c \
  libretro-common/formats/libchdr/libchdr_cdrom.c \
  libretro-common/formats/libchdr/libchdr_chd.c \
  libretro-common/formats/libchdr/libchdr_huffman.c \
  libretro-common/hash/lrc_hash.c \
  libretro-common/streams/chd_stream.c \
  libretro-common/streams/file_stream.c \
  libretro-common/streams/interface_stream.c \
  libretro-common/streams/memory_stream.c \
  libretro-common/string/stdstring.c \
  libretro-common/time/rtime.c \
  libretro-common/utils/md5.c \
  libretro-common/vfs/vfs_implementation.c \
  classicslive-integration/editor/cle_action_block.cpp \
  classicslive-integration/editor/cle_action_block_bookend.cpp \
  classicslive-integration/editor/cle_action_block_ctrbinary.cpp \
  classicslive-integration/editor/cle_common.cpp \
  classicslive-integration/editor/cle_hex_view.cpp \
  classicslive-integration/editor/cle_main.cpp \
  classicslive-integration/editor/cle_memory_inspector.cpp \
  classicslive-integration/editor/cle_memory_note_submit.cpp \
  classicslive-integration/editor/cle_result_table.cpp \
  classicslive-integration/editor/cle_result_table_normal.cpp \
  classicslive-integration/editor/cle_result_table_pointer.cpp \
  classicslive-integration/editor/cle_script_editor.cpp \
  classicslive-integration/editor/cle_script_editor_block.cpp \
  cl_frontend.cpp \
  classicslive-integration/cl_action.c \
  classicslive-integration/cl_common.c \
  classicslive-integration/cl_config.c \
  classicslive-integration/cl_counter.c \
  classicslive-integration/cl_identify.c \
  classicslive-integration/cl_json.c \
  classicslive-integration/cl_main.c \
  classicslive-integration/cl_memory.c \
  classicslive-integration/cl_network.c \
  classicslive-integration/cl_script.c \
  classicslive-integration/cl_search.c \
  cls_hook.cpp \
  cls_hook_cemu.cpp \
  cls_network_manager.cpp \
  cls_thread.cpp

HEADERS += \
  classicslive-integration/cl_action.h \
  classicslive-integration/cl_common.h \
  classicslive-integration/cl_config.h \
  classicslive-integration/cl_counter.h \
  classicslive-integration/cl_frontend.h \
  classicslive-integration/cl_identify.h \
  classicslive-integration/cl_json.h \
  classicslive-integration/cl_main.h \
  classicslive-integration/cl_memory.h \
  classicslive-integration/cl_network.h \
  classicslive-integration/cl_script.h \
  classicslive-integration/cl_search.h \
  classicslive-integration/cl_types.h \
  classicslive-integration/editor/cle_action_block.h \
  classicslive-integration/editor/cle_action_block_bookend.h \
  classicslive-integration/editor/cle_action_block_ctrbinary.h \
  classicslive-integration/editor/cle_common.h \
  classicslive-integration/editor/cle_hex_view.h \
  classicslive-integration/editor/cle_main.h \
  classicslive-integration/editor/cle_memory_inspector.h \
  classicslive-integration/editor/cle_memory_note_submit.h \
  classicslive-integration/editor/cle_result_table.h \
  classicslive-integration/editor/cle_result_table_normal.h \
  classicslive-integration/editor/cle_result_table_pointer.h \
  classicslive-integration/editor/cle_script_editor.h \
  classicslive-integration/editor/cle_script_editor_block.h \
  cls_hook.h \
  cls_hook_cemu.h \
  cls_hook_dialog.h \
  cls_hook_infuse.h \
  cls_hook_ryujinx.h \
  cls_hook_touchhle.h \
  cls_main.h \
  cls_network_manager.h \
  cls_thread.h
