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
#include <QProgressDialog>

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
    void onDeviceCreated(std::shared_ptr<Device> device);
    void onDeviceStatusUpdated(int index, Message msg);
    void onSelectVideoFile();
    void updateStartButtonState();

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
    bool prepareFFmpeg(QProgressDialog* progress);
    bool copyFileWithProgress(const QString& source, const QString& destination);
    bool checkFfmpegWorks(const QString& ffmpegPath);

    QTableWidget *m_deviceTable;
    QPushButton *m_startButton;
    QLineEdit *m_serverSipIdEdit;
    QLineEdit *m_serverIpEdit;
    QSpinBox *m_serverPortSpin;
    QLineEdit *m_passwordEdit;
    QSpinBox *m_deviceCountSpin;
    QPushButton* m_fileSelectButton;
    QLineEdit* m_filePathEdit;
    QString m_selectedFilePath;

    DeviceThread *m_deviceThread;
    QMap<int, std::shared_ptr<Device>> m_devices;
    bool m_isStarted;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H