#include "MainWindow.h"
#include "DeviceThread.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>
#include <pugixml.hpp>
#include <sstream>
#include <QFileDialog>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_deviceThread(new DeviceThread(this))
    , m_isStarted(false)
{
    setupUi();
    setupConnections();
    
    // Set default values
    m_serverSipIdEdit->setText("34020000002000000001");
    m_serverIpEdit->setText("127.0.0.1");
    m_serverPortSpin->setValue(5060);
    m_passwordEdit->setText("12345678");
    m_deviceCountSpin->setValue(1);
    
    // Load config
    loadConfig();
    
    // Refresh UI
    QCoreApplication::processEvents();
}

MainWindow::~MainWindow()
{
    if (m_isStarted) {
        stopDevices();
    }
}

void MainWindow::setupUi()
{
    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Configuration area
    QGroupBox *configGroup = new QGroupBox("Configuration", centralWidget);
    QFormLayout *configLayout = new QFormLayout(configGroup);

    m_serverSipIdEdit = new QLineEdit(configGroup);
    m_serverIpEdit = new QLineEdit(configGroup);
    m_serverPortSpin = new QSpinBox(configGroup);
    m_serverPortSpin->setRange(1, 65535);
    m_serverPortSpin->setValue(5060);
    m_passwordEdit = new QLineEdit(configGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_deviceCountSpin = new QSpinBox(configGroup);
    m_deviceCountSpin->setRange(1, 10000);
    m_deviceCountSpin->setValue(1);

    configLayout->addRow("Server SIP ID:", m_serverSipIdEdit);
    configLayout->addRow("Server IP:", m_serverIpEdit);
    configLayout->addRow("Server Port:", m_serverPortSpin);
    configLayout->addRow("Password:", m_passwordEdit);
    configLayout->addRow("Device Count:", m_deviceCountSpin);

    // Button area
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start", centralWidget);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startButton);

    // Device table
    m_deviceTable = new QTableWidget(centralWidget);
    m_deviceTable->setColumnCount(7);
    m_deviceTable->setHorizontalHeaderLabels(
        QStringList() << "Index" << "DeviceId" << "VideoChannelId" << "Local Port" 
                     << "Target Port" << "Protocol" << "Status");
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Add to main layout
    mainLayout->addWidget(configGroup);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_deviceTable);

    // Window settings
    resize(800, 600);
    setWindowTitle("GB28181 Stress Tools (Qt)");
}

void MainWindow::setupConnections()
{
    connect(m_deviceThread, &DeviceThread::deviceCreated, this, &MainWindow::onDeviceCreated);
    connect(m_deviceThread, &DeviceThread::deviceStatusUpdated, this, &MainWindow::onDeviceStatusUpdated);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
}

// Get config file path
QString MainWindow::getConfigFilePath()
{
    // Define source directory config file path (highest priority)
    QString sourcePath = "./config/config.xml";
    
    // If source directory config exists, use it
    if (QFile::exists(sourcePath)) {
        qDebug() << "Using source directory config file:" << sourcePath;
        return sourcePath;
    }
    
    // Other possible locations
    QStringList possiblePaths = {
        "./config.xml",                          // Current directory
        "config/config.xml",                     // Config subdirectory
        QCoreApplication::applicationDirPath() + "/config.xml"  // Application directory
    };
    
    // Try to find existing config file
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            qDebug() << "Found config file:" << path;
            return path;
        }
    }
    
    // If not found, use source directory
    qDebug() << "Config file not found, using source directory:" << sourcePath;
    return sourcePath;
}

void MainWindow::loadConfig()
{
    // Debug all possible file paths
    qDebug() << "Checking config file paths:";
    qDebug() << "  Current directory:" << QDir::currentPath() + "/config.xml" << QFile::exists(QDir::currentPath() + "/config.xml");
    qDebug() << "  Application directory:" << QCoreApplication::applicationDirPath() + "/config.xml" << QFile::exists(QCoreApplication::applicationDirPath() + "/config.xml");
    qDebug() << "  Project subdirectory:" << "GB28181_Stress_Tools/config.xml" << QFile::exists("GB28181_Stress_Tools/config.xml");
    
    QString configPath = getConfigFilePath();
    qDebug() << "Using config file:" << configPath;
    
    QFile file(configPath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open config file:" << configPath << "Error:" << file.errorString();
        // If config file not found, create default config
        saveConfig();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(data.data(), data.size());
    
    if (!result) {
        qDebug() << "XML parsing failed:" << result.description();
        return;
    }
    
    pugi::xml_node config = doc.child("config");
    if (!config) {
        qDebug() << "Config node not found";
        return;
    }
    
    // Read configuration
    QString serverSipId = config.child_value("serverId");
    QString serverIp = config.child_value("serverIp");
    QString serverPort = config.child_value("serverPort");
    QString password = config.child_value("password");
    QString count = config.child_value("count");
    
    qDebug() << "Parsed config values:";
    qDebug() << "  Server ID:" << serverSipId;
    qDebug() << "  Server IP:" << serverIp;
    qDebug() << "  Server Port:" << serverPort;
    qDebug() << "  Password:" << password;
    qDebug() << "  Device Count:" << count;
    
    // Set control values
    if (!serverSipId.isEmpty()) {
        m_serverSipIdEdit->setText(serverSipId);
        qDebug() << "Set server ID:" << serverSipId;
    }
    
    if (!serverIp.isEmpty()) {
        m_serverIpEdit->setText(serverIp);
        qDebug() << "Set server IP:" << serverIp;
    }
    
    if (!serverPort.isEmpty()) {
        bool ok;
        int port = serverPort.toInt(&ok);
        if (ok) {
            m_serverPortSpin->setValue(port);
            qDebug() << "Set server port:" << port;
        }
    }
    
    if (!password.isEmpty()) {
        m_passwordEdit->setText(password);
        qDebug() << "Set password:" << password;
    }
    
    if (!count.isEmpty()) {
        bool ok;
        int deviceCount = count.toInt(&ok);
        if (ok) {
            m_deviceCountSpin->setValue(deviceCount);
            qDebug() << "Set device count:" << deviceCount;
        }
    }
    
    qDebug() << "Config loaded from:" << configPath;
}

void MainWindow::saveConfig()
{
    // Use pugixml to create XML
    pugi::xml_document doc;
    pugi::xml_node config = doc.append_child("config");
    
    config.append_child("serverId").text().set(m_serverSipIdEdit->text().toStdString().c_str());
    config.append_child("serverIp").text().set(m_serverIpEdit->text().toStdString().c_str());
    config.append_child("serverPort").text().set(m_serverPortSpin->value());
    config.append_child("password").text().set(m_passwordEdit->text().toStdString().c_str());
    config.append_child("count").text().set(m_deviceCountSpin->value());
    
    // Get config file path
    QString configPath = "./config/config.xml";
    
    // Save config
    bool mainSaveResult = saveXmlToFile(doc, configPath);
    
    qDebug() << "Config saved";
}

bool MainWindow::saveXmlToFile(pugi::xml_document& doc, const QString& filePath)
{
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // Try direct save
    bool saveResult = doc.save_file(filePath.toStdString().c_str(), "  ");
    
    if (saveResult) {
        qDebug() << "Config saved to:" << filePath;
        return true;
    } 
    
    // If direct save failed, try Qt way
    qDebug() << "Direct save failed, trying Qt way:" << filePath;
    
    std::ostringstream oss;
    doc.save(oss, "  ");
    QString xmlContent = QString::fromStdString(oss.str());
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << xmlContent;
        file.close();
        qDebug() << "Using Qt way save succeeded:" << filePath;
        return true;
    } else {
        qDebug() << "All save ways failed:" << filePath << file.errorString();
        return false;
    }
}

void MainWindow::onStartButtonClicked()
{
    if (!m_isStarted) {
        // If not started, perform start operations
        if (!checkParams()) {
            return;
        }
        
        saveConfig();
        
        // Start devices
        startDevices();
        m_startButton->setText("Stop");
        m_isStarted = true;
        
        qDebug() << "Devices started: " << m_deviceCountSpin->value();
    } else {
        // If already started, perform stop operations
        stopDevices();
        m_startButton->setText("Start");
        m_isStarted = false;
        
        qDebug() << "Devices stopped";
    }
}

bool MainWindow::checkParams()
{
    if (m_serverSipIdEdit->text().length() < 10) {
        QMessageBox::warning(this, "Warning", "Server SIP ID must be at least 10 characters");
        return false;
    }

    if (m_serverIpEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter Server IP");
        return false;
    }

    if (m_passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Password cannot be empty");
        return false;
    }

    return true;
}

void MainWindow::startDevices()
{
    // Clear device table
    m_deviceTable->setRowCount(0);
    m_devices.clear();
    
    int deviceCount = m_deviceCountSpin->value();
    
    // Start device thread
    m_deviceThread->start(
        m_serverSipIdEdit->text(),
        m_serverIpEdit->text(),
        m_serverPortSpin->value(),
        m_passwordEdit->text(),
        deviceCount
    );
}

void MainWindow::stopDevices()
{
    if (m_deviceThread != nullptr && m_deviceThread->isRunning()) {
        m_deviceThread->stop();
        m_deviceThread->wait();
    }
    
    m_devices.clear();
}

void MainWindow::onDeviceCreated(std::shared_ptr<Device> device)
{
    int row = m_deviceTable->rowCount();
    m_deviceTable->insertRow(row);
    
    // Set device ID
    m_deviceTable->setItem(row, 0, new QTableWidgetItem(QString::number(device->list_index + 1)));
    m_deviceTable->setItem(row, 1, new QTableWidgetItem(QString::number(device->list_index + 1)));
    m_deviceTable->setItem(row, 2, new QTableWidgetItem(QString::number(device->list_index + 1)));
    m_deviceTable->setItem(row, 3, new QTableWidgetItem(QString::number(5060 + device->list_index + 1)));
    m_deviceTable->setItem(row, 4, new QTableWidgetItem(QString::number(m_serverPortSpin->value())));
    m_deviceTable->setItem(row, 5, new QTableWidgetItem("--"));
    m_deviceTable->setItem(row, 6, new QTableWidgetItem("Initializing"));
    
    // Save device pointer
    m_devices[device->list_index] = device;
}

void MainWindow::onDeviceStatusUpdated(int index, Message msg)
{
    // Update table device status
    for (int row = 0; row < m_deviceTable->rowCount(); row++) {
        if (m_deviceTable->item(row, 0)->text().toInt() == index + 1) {
            switch (msg.type) {
                case STATUS_TYPE:
                    m_deviceTable->setItem(row, 6, new QTableWidgetItem(QString(msg.content)));
                    break;
                case PULL_STREAM_PROTOCOL_TYPE:
                    m_deviceTable->setItem(row, 5, new QTableWidgetItem(QString(msg.content)));
                    break;
                case PULL_STREAM_PORT_TYPE:
                    m_deviceTable->setItem(row, 4, new QTableWidgetItem(QString(msg.content)));
                    break;
            }
            break;
        }
    }
} 