#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <memory>
#include <vector>
#include "../common/Device.h"

class DeviceThread : public QThread
{
    Q_OBJECT

public:
    DeviceThread(QObject *parent = nullptr);
    ~DeviceThread();

    void start(const QString& serverSipId, const QString& serverIp, int serverPort, 
               const QString& password, int deviceCount);
    void stop();

protected:
    void run() override;

signals:
    void deviceCreated(std::shared_ptr<Device> device);
    void deviceStatusUpdated(int index, Message msg);

private:
    QString m_serverSipId;
    QString m_serverIp;
    int m_serverPort;
    QString m_password;
    int m_deviceCount;
    bool m_isRunning;
    QMutex m_mutex;
    
    // Store device pointers for proper cleanup
    std::vector<std::shared_ptr<Device>> m_devices;
}; 