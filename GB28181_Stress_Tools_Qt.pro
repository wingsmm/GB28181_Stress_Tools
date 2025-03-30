QT += core gui widgets network

CONFIG += c++11

TARGET = GB28181_Stress_Tools_Qt
TEMPLATE = app

# 定义
DEFINES += QT_DEPRECATED_WARNINGS _AFXDLL USE_QT_VERSION _CRT_SECURE_NO_WARNINGS

# 包含所有源文件
SOURCES += \
    main_qt.cpp \
    MainWindow.cpp \
    DeviceThread.cpp \
    Device_qt.cpp \
    GB28181_Stress_Tools/LoadH264.cpp \
    GB28181_Stress_Tools/NaluProvider.cpp \
    GB28181_Stress_Tools/UDPClient.cpp \
    GB28181_Stress_Tools/gb28181_header_maker.cpp \
    GB28181_Stress_Tools/h264_parser.c \
    GB28181_Stress_Tools/pugixml.cpp

HEADERS += \
    MainWindow.h \
    DeviceThread.h \
    qt_adapters.h \
    GB28181_Stress_Tools/Message.h \
    GB28181_Stress_Tools/Device.h \
    GB28181_Stress_Tools/HexStringUtils.h \
    GB28181_Stress_Tools/LoadH264.h \
    GB28181_Stress_Tools/Nalu.h \
    GB28181_Stress_Tools/NaluProvider.h \
    GB28181_Stress_Tools/NaluType.h \
    GB28181_Stress_Tools/UDPClient.h \
    GB28181_Stress_Tools/bits.h \
    GB28181_Stress_Tools/gb28181_header_maker.h \
    GB28181_Stress_Tools/h264_parser.h \
    GB28181_Stress_Tools/pugiconfig.hpp \
    GB28181_Stress_Tools/pugixml.hpp

# 包含头文件路径
INCLUDEPATH += \
    . \
    ./GB28181_Stress_Tools \
    ./include \
    ./include/ffmpeg \
    ./include/ffmpeg/libavcodec \
    ./include/ffmpeg/libavformat \
    ./include/ffmpeg/libavutil \
    ./include/ffmpeg/libswscale

# 库文件路径设置 - 使用qmake变量和绝对路径
LIB_PATH = $$PWD/lib
win32:LIB_PATH_WIN = D:/Developer/git/GB28181_Stress_Tools/lib

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
    resources.qrc

# 禁用一些警告
QMAKE_CXXFLAGS += -wd4819 -wd4311 -wd4302

# 复制配置文件到编译目录
CONFIG_FILE = $$PWD/config.xml
CONFIG_FILE_ALT = $$PWD/GB28181_Stress_Tools/config.xml

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
        
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_WIN} $${CONFIG_DEST_WIN}$$escape_expand(\n\t))
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_WIN} $${CONFIG_DEST_RELEASE_WIN}$$escape_expand(\n\t))
    }
} else {
    exists($$CONFIG_FILE_ALT) {
        win32 {
            CONFIG_FILE_ALT_WIN = $${CONFIG_FILE_ALT}
            CONFIG_FILE_ALT_WIN ~= s,/,\\,g
            CONFIG_DEST_WIN = $${CONFIG_DEST}
            CONFIG_DEST_WIN ~= s,/,\\,g
            CONFIG_DEST_RELEASE_WIN = $${CONFIG_DEST_RELEASE}
            CONFIG_DEST_RELEASE_WIN ~= s,/,\\,g
            
            QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_ALT_WIN} $${CONFIG_DEST_WIN}\\config.xml$$escape_expand(\n\t))
            QMAKE_POST_LINK += $$quote(cmd /c copy /y $${CONFIG_FILE_ALT_WIN} $${CONFIG_DEST_RELEASE_WIN}\\config.xml$$escape_expand(\n\t))
        }
    }
}

# 调试信息
message(PWD is $$PWD)
message(LIB_PATH is $$LIB_PATH)
win32:message(LIB_PATH_WIN is $$LIB_PATH_WIN) 