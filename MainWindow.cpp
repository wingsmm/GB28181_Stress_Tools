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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_isStarted(false), m_deviceThread(nullptr)
{
    setupUi();
    // 先设置默认值，然后加载配置覆盖
    m_serverSipIdEdit->setText("34020000002000000001");
    m_serverIpEdit->setText("127.0.0.1");
    m_serverPortSpin->setValue(5060);
    m_passwordEdit->setText("12345678");
    m_deviceCountSpin->setValue(1);
    
    // 加载配置
    loadConfig();
    
    // 刷新UI以确保值显示
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
    // 创建主窗口部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 配置区域
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

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("Start", centralWidget);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startButton);

    // 设备表格
    m_deviceTable = new QTableWidget(centralWidget);
    m_deviceTable->setColumnCount(7);
    m_deviceTable->setHorizontalHeaderLabels(
        QStringList() << "Index" << "DeviceId" << "VideoChannelId" << "Local Port" 
                     << "Target Port" << "Protocol" << "Status");
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 添加到主布局
    mainLayout->addWidget(configGroup);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_deviceTable);

    // 窗口设置
    resize(800, 600);
    setWindowTitle("GB28181 Stress Tools (Qt)");
}

// 获取配置文件路径
QString MainWindow::getConfigFilePath()
{
    // 定义源码目录的配置文件路径（固定路径，优先级最高）
    QString sourcePath = "../GB28181_Stress_Tools/config.xml";
    
    // 如果源码目录的配置文件存在，直接使用它
    if (QFile::exists(sourcePath)) {
        qDebug() << "使用源码目录的配置文件:" << sourcePath;
        return sourcePath;
    }
    
    // 其他可能的位置
    QStringList possiblePaths = {
        "config.xml",                            // 当前目录
        "GB28181_Stress_Tools/config.xml",       // 项目子目录
        QCoreApplication::applicationDirPath() + "/config.xml"  // 应用程序目录
    };
    
    // 尝试查找已存在的配置文件
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            qDebug() << "找到配置文件:" << path;
            return path;
        }
    }
    
    // 如果没有找到，使用源码目录作为保存位置
    qDebug() << "未找到配置文件，将使用源码目录:" << sourcePath;
    return sourcePath;
}

void MainWindow::loadConfig()
{
    // 调试所有可能的文件路径
    qDebug() << "检查配置文件路径:";
    qDebug() << "当前目录:" << QDir::currentPath() + "/config.xml" << QFile::exists(QDir::currentPath() + "/config.xml");
    qDebug() << "程序目录:" << QCoreApplication::applicationDirPath() + "/config.xml" << QFile::exists(QCoreApplication::applicationDirPath() + "/config.xml");
    qDebug() << "项目子目录:" << "GB28181_Stress_Tools/config.xml" << QFile::exists("GB28181_Stress_Tools/config.xml");
    
    QString configPath = getConfigFilePath();
    qDebug() << "将使用配置文件:" << configPath;
    
    QFile file(configPath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开配置文件:" << configPath << "错误:" << file.errorString();
        // 如果找不到配置文件，则创建默认配置
        saveConfig();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // 使用pugixml解析XML
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(data.data(), data.size());
    
    if (!result) {
        qDebug() << "XML解析失败:" << result.description();
        return;
    }
    
    pugi::xml_node config = doc.child("config");
    if (!config) {
        qDebug() << "找不到config节点";
        return;
    }
    
    // 读取配置
    QString serverSipId = config.child_value("serverId");
    QString serverIp = config.child_value("serverIp");
    QString serverPort = config.child_value("serverPort");
    QString password = config.child_value("password");
    QString count = config.child_value("count");
    
    qDebug() << "解析的配置值:";
    qDebug() << "  serverId:" << serverSipId;
    qDebug() << "  serverIp:" << serverIp;
    qDebug() << "  serverPort:" << serverPort;
    qDebug() << "  password:" << password;
    qDebug() << "  count:" << count;
    
    // 设置控件值
    if (!serverSipId.isEmpty()) {
        m_serverSipIdEdit->setText(serverSipId);
        qDebug() << "设置服务器ID:" << serverSipId;
    }
    
    if (!serverIp.isEmpty()) {
        m_serverIpEdit->setText(serverIp);
        qDebug() << "设置服务器IP:" << serverIp;
    }
    
    if (!serverPort.isEmpty()) {
        bool ok;
        int port = serverPort.toInt(&ok);
        if (ok) {
            m_serverPortSpin->setValue(port);
            qDebug() << "设置服务器端口:" << port;
        }
    }
    
    if (!password.isEmpty()) {
        m_passwordEdit->setText(password);
        qDebug() << "设置密码:" << password;
    }
    
    if (!count.isEmpty()) {
        bool ok;
        int deviceCount = count.toInt(&ok);
        if (ok) {
            m_deviceCountSpin->setValue(deviceCount);
            qDebug() << "设置设备数量:" << deviceCount;
        }
    }
    
    qDebug() << "配置加载完成，来自:" << configPath;
}

void MainWindow::saveConfig()
{
    // 使用pugixml创建XML
    pugi::xml_document doc;
    pugi::xml_node config = doc.append_child("config");
    
    config.append_child("serverId").text().set(m_serverSipIdEdit->text().toStdString().c_str());
    config.append_child("serverIp").text().set(m_serverIpEdit->text().toStdString().c_str());
    config.append_child("serverPort").text().set(m_serverPortSpin->value());
    config.append_child("password").text().set(m_passwordEdit->text().toStdString().c_str());
    config.append_child("count").text().set(m_deviceCountSpin->value());
    
    // 获取配置文件路径
    QString configPath = getConfigFilePath();
    
    // 尝试同时保存到源码目录，确保源码和运行目录同步
    QString sourcePath = "../GB28181_Stress_Tools/config.xml";
    
    // 保存配置
    bool mainSaveResult = saveXmlToFile(doc, configPath);
    
    // 如果配置路径不是源码目录，则同时保存到源码目录
    if (configPath != sourcePath) {
        bool sourceSaveResult = saveXmlToFile(doc, sourcePath);
        if (sourceSaveResult) {
            qDebug() << "已同步保存配置到源码目录:" << sourcePath;
        }
    }
    
    qDebug() << "配置保存完成";
}

bool MainWindow::saveXmlToFile(pugi::xml_document& doc, const QString& filePath)
{
    // 确保目录存在
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // 尝试直接保存
    bool saveResult = doc.save_file(filePath.toStdString().c_str(), "  ");
    
    if (saveResult) {
        qDebug() << "配置已保存到:" << filePath;
        return true;
    } 
    
    // 如果直接保存失败，尝试Qt方式
    qDebug() << "直接保存失败，尝试Qt方式:" << filePath;
    
    std::ostringstream oss;
    doc.save(oss, "  ");
    QString xmlContent = QString::fromStdString(oss.str());
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << xmlContent;
        file.close();
        qDebug() << "使用Qt方式保存成功:" << filePath;
        return true;
    } else {
        qDebug() << "所有保存方式都失败:" << filePath << file.errorString();
        return false;
    }
}

void MainWindow::onStartButtonClicked()
{
    if (!m_isStarted) {
        if (!checkParams()) {
            return;
        }
        
        saveConfig();
        
        // 开始设备
        startDevices();
        m_startButton->setText("Stop");
        m_isStarted = true;
    } else {
        // 停止设备
        stopDevices();
        m_startButton->setText("Start");
        m_isStarted = false;
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
    // 清空设备表格
    m_deviceTable->setRowCount(0);
    m_deviceVector.clear();
    
    int deviceCount = m_deviceCountSpin->value();
    // 准备表格
    m_deviceTable->setRowCount(deviceCount);
    
    // 创建线程
    if (m_deviceThread == nullptr) {
        m_deviceThread = new DeviceThread(this);
        connect(m_deviceThread, &DeviceThread::deviceStatus, this, &MainWindow::updateDeviceStatus);
        connect(m_deviceThread, &DeviceThread::deviceCreated, this, &MainWindow::deviceCreated);
    }
    
    // 设置参数
    m_deviceThread->setParameters(
        m_serverSipIdEdit->text(),
        m_serverIpEdit->text(),
        m_serverPortSpin->value(),
        m_passwordEdit->text(),
        deviceCount
    );
    
    // 启动线程
    m_deviceThread->start();
}

void MainWindow::stopDevices()
{
    if (m_deviceThread != nullptr && m_deviceThread->isRunning()) {
        m_deviceThread->stop();
        m_deviceThread->wait();
    }
    
    m_deviceVector.clear();
}

void MainWindow::updateDeviceStatus(int index, Message msg)
{
    if (index >= m_deviceTable->rowCount()) {
        return;
    }
    
    if (msg.type == STATUS_TYPE) {
        m_deviceTable->setItem(index, 6, new QTableWidgetItem(QString(msg.content)));
    } else if (msg.type == PULL_STREAM_PROTOCOL_TYPE) {
        m_deviceTable->setItem(index, 5, new QTableWidgetItem(QString(msg.content)));
    } else if (msg.type == PULL_STREAM_PORT_TYPE) {
        m_deviceTable->setItem(index, 4, new QTableWidgetItem(QString(msg.content)));
    }
}

void MainWindow::deviceCreated(std::shared_ptr<Device> device)
{
    int index = device->list_index;
    m_deviceVector.append(device);
    
    // 设置表格初始数据
    m_deviceTable->setItem(index, 0, new QTableWidgetItem(QString::number(index + 1)));
} 