#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QVector>
#include <QThread>
#include <memory>
#include "Device.h"
#include "Message.h"
#include <pugixml.hpp>

class DeviceThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateDeviceStatus(int index, Message msg);
    void deviceCreated(std::shared_ptr<Device> device);

private slots:
    void onStartButtonClicked();

private:
    void setupUi();
    bool checkParams();
    void startDevices();
    void stopDevices();
    void loadConfig();
    void saveConfig();
    QString getConfigFilePath();
    bool saveXmlToFile(pugi::xml_document& doc, const QString& filePath);
    
private:
    QLineEdit *m_serverSipIdEdit;
    QLineEdit *m_serverIpEdit;
    QSpinBox *m_serverPortSpin;
    QLineEdit *m_passwordEdit;
    QSpinBox *m_deviceCountSpin;
    QPushButton *m_startButton;
    QTableWidget *m_deviceTable;

    bool m_isStarted;
    QVector<std::shared_ptr<Device>> m_deviceVector;
    DeviceThread *m_deviceThread;
}; 