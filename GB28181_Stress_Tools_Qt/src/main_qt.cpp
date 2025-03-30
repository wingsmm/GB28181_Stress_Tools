#include <QApplication>
#include <QFile>
#include "MainWindow.h"
#include "Device.h"
#include "Message.h"

// 为Message类型定义Q_DECLARE_METATYPE，使其可注册
Q_DECLARE_METATYPE(Message)
// 为std::shared_ptr<Device>定义Q_DECLARE_METATYPE
Q_DECLARE_METATYPE(std::shared_ptr<Device>)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 注册自定义类型，用于跨线程信号槽
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<std::shared_ptr<Device>>("std::shared_ptr<Device>");
    
    // 设置应用程序信息
    QApplication::setApplicationName("GB28181 Stress Tools (Qt)");
    QApplication::setOrganizationName("GB28181");
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
} 