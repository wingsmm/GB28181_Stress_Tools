# 设置UTF-8编码支持
CODECFORTR = UTF-8
CODECFORSRC = UTF-8

QT += core gui widgets network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = GB28181_Stress_Tools_Qt
TEMPLATE = app

# 禁用编码警告 (4819是关于中文编码的警告)
win32: QMAKE_CXXFLAGS += -wd4819 /utf-8

# 定义
DEFINES += QT_DEPRECATED_WARNINGS _AFXDLL USE_QT_VERSION _CRT_SECURE_NO_WARNINGS

# 设置C和C++源文件
SOURCES_CPP = \
    src/main.cpp \
    src/ui/MainWindow.cpp \
    src/core/DeviceThread.cpp \
    src/common/Device.cpp \
    src/common/LoadH264.cpp \
    src/common/NaluProvider.cpp \
    src/common/UDPClient.cpp \
    src/common/gb28181_header_maker.cpp \
    src/common/pugixml.cpp \
    src/common/FFmpegLoader.cpp

SOURCES_C = \
    src/common/h264_parser.c

# 包含所有源文件
SOURCES = $$SOURCES_CPP $$SOURCES_C

HEADERS += \
    src/ui/MainWindow.h \
    src/core/DeviceThread.h \
    src/common/Message.h \
    src/common/Device.h \
    src/common/HexStringUtils.h \
    src/common/LoadH264.h \
    src/common/Nalu.h \
    src/common/NaluProvider.h \
    src/common/NaluType.h \
    src/common/UDPClient.h \
    src/common/bits.h \
    src/common/gb28181_header_maker.h \
    src/common/h264_parser.h \
    src/common/pugiconfig.hpp \
    src/common/pugixml.hpp \
    src/common/FFmpegLoader.h

# 为C文件设置编译标志
QMAKE_CFLAGS += -x c

# 包含头文件路径
INCLUDEPATH += \
    . \
    $$PWD/src \
    $$PWD/src/ui \
    $$PWD/src/core \
    $$PWD/src/common \
    $$PWD/../include \
    $$PWD/../include/ffmpeg \
    $$PWD/../include/ffmpeg/libavcodec \
    $$PWD/../include/ffmpeg/libavformat \
    $$PWD/../include/ffmpeg/libavutil \
    $$PWD/../include/ffmpeg/libswscale \
    $$[QT_INSTALL_HEADERS] \
    $$[QT_INSTALL_HEADERS]/QtCore \
    $$[QT_INSTALL_HEADERS]/QtGui \
    $$[QT_INSTALL_HEADERS]/QtWidgets \
    $$[QT_INSTALL_HEADERS]/QtNetwork \
    $$PWD/ffmpeg/include

# 库文件路径设置
LIB_PATH = $$PWD/../lib
win32:LIB_PATH_WIN = $$PWD/../lib

# 添加库文件路径
LIBS += -L$$LIB_PATH
win32:LIBS += -L$$LIB_PATH_WIN

# 添加库文件
LIBS += -leXosip -losip2 -losipparser2 -llibcares

# Windows特定库
win32 {
    LIBS += -lws2_32 -lDnsapi -lIphlpapi -lQwave -ldelayimp -lAdvapi32
}

# 资源文件
RESOURCES += \
    resources/resources.qrc

# 禁用一些警告
QMAKE_CXXFLAGS += -wd4819 -wd4311 -wd4302 -wd4099

# 添加链接器选项以抑制警告
win32 {
    # 忽略PDB文件缺失警告(LNK4099)
    QMAKE_LFLAGS += /IGNORE:4099
    
    # 解决默认库冲突警告(LNK4098)
    QMAKE_LFLAGS += /NODEFAULTLIB:LIBCMTD
}

# 复制配置文件到编译目录
CONFIG_FILE = $$PWD/config/config.xml

# 定义目标目录
win32 {
    CONFIG_DEST = $$OUT_PWD/debug
    CONFIG_DEST_RELEASE = $$OUT_PWD/release
}

# 检查配置文件存在并复制
exists($$CONFIG_FILE) {
    win32 {
        CONFIG_FILE_WIN = $${CONFIG_FILE}
        CONFIG_FILE_WIN ~= s,/,\\,g
        CONFIG_DEST_WIN = $${CONFIG_DEST}
        CONFIG_DEST_WIN ~= s,/,\\,g
        CONFIG_DEST_RELEASE_WIN = $${CONFIG_DEST_RELEASE}
        CONFIG_DEST_RELEASE_WIN ~= s,/,\\,g
        
        # 复制到debug目录
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_WIN} $${CONFIG_DEST_WIN}\\config.xml$$escape_expand(\n\t))
        
        # 复制到release目录
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_WIN} $${CONFIG_DEST_RELEASE_WIN}\\config.xml$$escape_expand(\n\t))
    }
}

# 修改resources.qrc中的路径
resources.files = resources/resources.qrc
resources.prefix = /

# 调试信息
message(PWD is $$PWD)
message(LIB_PATH is $$LIB_PATH)
win32:message(LIB_PATH_WIN is $$LIB_PATH_WIN)
