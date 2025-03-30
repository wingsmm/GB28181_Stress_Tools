#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <memory>
#include "Device.h"

class DeviceThread : public QThread
{
    Q_OBJECT

public:
    DeviceThread(QObject *parent = nullptr);
    ~DeviceThread();

    void setParameters(const QString &serverSipId, const QString &serverIp, 
                      int serverPort, const QString &password, int deviceCount);
    void stop();

protected:
    void run() override;

signals:
    void deviceStatus(int index, Message msg);
    void deviceCreated(std::shared_ptr<Device> device);

private:
    QString m_serverSipId;
    QString m_serverIp;
    int m_serverPort;
    QString m_password;
    int m_deviceCount;
    bool m_isRunning;
    QMutex m_mutex;
}; 