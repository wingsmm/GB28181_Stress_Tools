#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "MainWindow.h"
#include "Device.h"
#include "Message.h"

// 为Message类型定义Q_DECLARE_METATYPE，使其可注册
Q_DECLARE_METATYPE(Message)
// 为std::shared_ptr<Device>定义Q_DECLARE_METATYPE
Q_DECLARE_METATYPE(std::shared_ptr<Device>)

// 自定义streambuf类，将输出重定向到Qt的日志系统
class QtStreamBuf : public std::streambuf {
public:
    QtStreamBuf() : std::streambuf() {
        buffer.reserve(1024);  // 预分配缓冲区
    }
    
protected:
    virtual int_type overflow(int_type v) override {
        if (v != traits_type::eof()) {
            char_type ch = traits_type::to_char_type(v);
            if (ch == '\n') {
                flushBuffer();
            } else {
                buffer += ch;
            }
        }
        return v;
    }
    
    virtual std::streamsize xsputn(const char_type* p, std::streamsize count) override {
        QString str = QString::fromUtf8(p, count);
        if (str.contains('\n')) {
            QStringList lines = str.split('\n');
            for (int i = 0; i < lines.size(); ++i) {
                if (!lines[i].isEmpty()) {
                    qDebug().noquote() << lines[i];
                }
            }
        } else {
            buffer += str;
        }
        return count;
    }

private:
    void flushBuffer() {
        if (!buffer.isEmpty()) {
            qDebug().noquote() << buffer;
            buffer.clear();
        }
    }

    QString buffer;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 注册自定义类型，用于跨线程信号槽
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<std::shared_ptr<Device>>("std::shared_ptr<Device>");
    
    // 设置应用程序信息
    QApplication::setApplicationName("GB28181 Stress Tools (Qt)");
    QApplication::setOrganizationName("GB28181");
    
    // 重定向标准输出到Qt日志系统
    static QtStreamBuf qtStreamBuf;
    std::cout.rdbuf(&qtStreamBuf);
    std::cerr.rdbuf(&qtStreamBuf);
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
} 