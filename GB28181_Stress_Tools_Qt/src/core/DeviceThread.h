#pragma once

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QString>
#include <memory>
#include <vector>
#include "../common/Device.h"
#include "../common/Message.h"

class DeviceThread : public QThread
{
    Q_OBJECT

public:
    explicit DeviceThread(QObject *parent = nullptr);
    ~DeviceThread();

    void start(const QString& serverSipId, const QString& serverIp, int serverPort, 
               const QString& password, int deviceCount);
    void stop();
    void setVideoPath(const QString& videoPath);

protected:
    virtual void run() override;

signals:
    void deviceCreated(std::shared_ptr<Device> device);
    void deviceStatusUpdated(int index, Message message);

private:
    QMutex m_mutex;
    bool m_isRunning;
    QString m_serverSipId;
    QString m_serverIp;
    int m_serverPort;
    QString m_password;
    int m_deviceCount;
    std::vector<std::shared_ptr<Device>> m_devices;
    QString m_videoPath;  // 存储选择的视频文件路径
}; 