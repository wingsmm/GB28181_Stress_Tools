#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QMap>
#include <QDateTime>
#include "DeviceThread.h"
#include "Device.h"
#include "Message.h"
#include <pugixml.hpp>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onBrowseButtonClicked();
    void onDeviceCreated(std::shared_ptr<Device> device);
    void onDeviceStatusUpdated(int index, Message msg);

private:
    void setupUi();
    void setupConnections();
    void loadConfig();
    void saveConfig();
    bool checkParams();
    void startDevices();
    void stopDevices();
    QString getConfigFilePath();
    bool saveXmlToFile(pugi::xml_document& doc, const QString& filePath);

    QTableWidget *m_deviceTable;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_browseButton;
    QLineEdit *m_serverSipIdEdit;
    QLineEdit *m_serverIpEdit;
    QSpinBox *m_serverPortSpin;
    QLineEdit *m_passwordEdit;
    QSpinBox *m_deviceCountSpin;
    QLineEdit *m_configPathEdit;

    DeviceThread *m_deviceThread;
    QMap<int, std::shared_ptr<Device>> m_devices;
    bool m_isStarted;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H 