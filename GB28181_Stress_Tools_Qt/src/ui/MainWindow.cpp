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
#include <QProgressDialog>
#include "../common/FFmpegLoader.h"

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
    QGroupBox *configGroup = new QGroupBox("配置", centralWidget);
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

    configLayout->addRow("服务器SIP ID:", m_serverSipIdEdit);
    configLayout->addRow("服务器IP:", m_serverIpEdit);
    configLayout->addRow("服务器端口:", m_serverPortSpin);
    configLayout->addRow("密码:", m_passwordEdit);
    configLayout->addRow("设备数量:", m_deviceCountSpin);

    // Button area
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_startButton = new QPushButton("开始", centralWidget);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startButton);

    // Device table
    m_deviceTable = new QTableWidget(centralWidget);
    m_deviceTable->setColumnCount(7);
    m_deviceTable->setHorizontalHeaderLabels(
        QStringList() << "序号" << "设备ID" << "视频通道ID" << "本地端口" 
                     << "目标端口" << "协议" << "状态");
    m_deviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Add to main layout
    mainLayout->addWidget(configGroup);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_deviceTable);

    // 创建文件选择按钮和文件路径显示
    m_fileSelectButton = new QPushButton("选择视频文件", this);
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText("请选择MP4或H264文件");
    
    // 添加到布局
    QHBoxLayout* fileSelectLayout = new QHBoxLayout();
    fileSelectLayout->addWidget(m_fileSelectButton);
    fileSelectLayout->addWidget(m_filePathEdit);
    mainLayout->addLayout(fileSelectLayout);
    
    // 连接信号槽
    connect(m_fileSelectButton, &QPushButton::clicked, this, &MainWindow::onSelectVideoFile);
    connect(m_filePathEdit, &QLineEdit::textChanged, this, &MainWindow::updateStartButtonState);
    
    // 初始化启动按钮为禁用状态
    m_startButton->setEnabled(false);

    // Window settings
    resize(800, 600);
    setWindowTitle("视频平台");
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
    // 使用应用程序目录下的配置文件
    QString configPath = QCoreApplication::applicationDirPath() + "/config.xml";
    qDebug() << "使用配置文件:" << configPath;
    return configPath;
}

void MainWindow::loadConfig()
{
    QString configPath = getConfigFilePath();
    qDebug() << "正在加载配置文件:" << configPath;
    
    QFile file(configPath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开配置文件:" << configPath << "错误:" << file.errorString();
        // 如果配置文件不存在，创建默认配置
        saveConfig();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(data.data(), data.size());
    
    if (!result) {
        qDebug() << "XML解析失败:" << result.description();
        return;
    }
    
    pugi::xml_node config = doc.child("config");
    if (!config) {
        qDebug() << "未找到配置节点";
        return;
    }
    
    // Read configuration
    QString serverSipId = config.child_value("serverId");
    QString serverIp = config.child_value("serverIp");
    QString serverPort = config.child_value("serverPort");
    QString password = config.child_value("password");
    QString count = config.child_value("count");
    
    qDebug() << "解析的配置值:";
    qDebug() << "  服务器ID:" << serverSipId;
    qDebug() << "  服务器IP:" << serverIp;
    qDebug() << "  服务器端口:" << serverPort;
    qDebug() << "  密码:" << password;
    qDebug() << "  设备数量:" << count;
    
    // Set control values
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
    
    qDebug() << "已从以下位置加载配置:" << configPath;
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
    QString configPath = getConfigFilePath();
    
    // Save config
    bool mainSaveResult = saveXmlToFile(doc, configPath);
    
    qDebug() << "配置已保存到:" << configPath;
}

bool MainWindow::saveXmlToFile(pugi::xml_document& doc, const QString& filePath)
{
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // Try direct save
    bool saveResult = doc.save_file(filePath.toStdString().c_str(), "  ");
    
    if (saveResult) {
        qDebug() << "配置已保存到:" << filePath;
        return true;
    } 
    
    // If direct save failed, try Qt way
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
        // If not started, perform start operations
        if (!checkParams()) {
            return;
        }
        
        saveConfig();
        
        // Start devices
        startDevices();
        m_startButton->setText("停止");
        m_isStarted = true;
        
        qDebug() << "已启动设备数量: " << m_deviceCountSpin->value();
    } else {
        // If already started, perform stop operations
        stopDevices();
        m_startButton->setText("开始");
        m_isStarted = false;
        
        qDebug() << "设备已停止";
    }
}

bool MainWindow::checkParams()
{
    if (m_serverSipIdEdit->text().length() < 10) {
        QMessageBox::warning(this, "警告", "服务器SIP ID至少需要10个字符");
        return false;
    }

    if (m_serverIpEdit->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入服务器IP");
        return false;
    }

    if (m_passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "密码不能为空");
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
    m_deviceThread->setVideoPath(m_selectedFilePath);
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

// 实现文件选择函数
void MainWindow::onSelectVideoFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "选择视频文件", QDir::homePath(),
        "视频文件 (*.mp4 *.h264 *.264);;MP4文件 (*.mp4);;H264文件 (*.h264 *.264);;所有文件 (*.*)"
    );
    
    if (filePath.isEmpty()) {
        return;  // 用户取消了选择
    }
    
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString baseName = fileInfo.baseName();
    QString extension = fileInfo.suffix().toLower();
    QString exeDir = QCoreApplication::applicationDirPath();
    
    // 检查是否为支持的格式
    if (extension != "mp4" && extension != "h264" && extension != "264") {
        QMessageBox::warning(this, "文件格式错误", 
            "目前只支持MP4和H264格式文件。\n请选择正确的文件格式。");
        return;
    }
    
    // 如果是H264文件，复制到exe同级目录
    if (extension == "h264" || extension == "264") {
        QString targetH264Path = exeDir + "/" + fileName;
        
        // 如果源文件已经在目标位置，则不需要复制
        if (filePath == targetH264Path) {
            m_selectedFilePath = filePath;
            m_filePathEdit->setText(fileName);
            m_startButton->setEnabled(true);
            QMessageBox::information(this, "文件准备完成", 
                "H264文件已准备就绪，可以开始推流。");
            return;
        }
        
        // 显示复制进度对话框
        QProgressDialog progress("正在复制H264文件...", "取消", 0, 100, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);
        progress.setValue(10);
        
        // 删除目标位置可能存在的同名文件
        if (QFile::exists(targetH264Path)) {
            QFile::remove(targetH264Path);
        }
        
        // 复制文件
        progress.setValue(50);
        if (QFile::copy(filePath, targetH264Path)) {
            progress.setValue(100);
            m_selectedFilePath = targetH264Path;
            m_filePathEdit->setText(fileName);
            m_startButton->setEnabled(true);
            QMessageBox::information(this, "文件准备完成", 
                "H264文件已复制到程序目录，可以开始推流。");
        } else {
            progress.cancel();
            QMessageBox::critical(this, "文件复制失败", 
                "无法复制H264文件到程序目录。");
        }
        
        return;
    }
    
    // 处理MP4文件
    if (extension == "mp4") {
        QString targetMP4Path = exeDir + "/" + fileName;
        QString h264FileName = baseName + ".h264";
        QString targetH264Path = exeDir + "/" + h264FileName;
        
        QProgressDialog *progress = new QProgressDialog("正在准备视频文件...", "取消", 0, 100, this);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(true);
        progress->setValue(10);
        
        // 如果目标H264已存在，询问是否使用
        if (QFile::exists(targetH264Path)) {
            progress->setValue(30);
            progress->setLabelText("检测到H264文件已存在...");
            QCoreApplication::processEvents();
            
            QMessageBox::StandardButton reply = QMessageBox::question(this, "文件已存在",
                "程序目录中已存在同名H264文件，是否直接使用？\n\n"
                "选择\"是\"：使用已有H264文件\n"
                "选择\"否\"：重新转换MP4文件",
                QMessageBox::Yes | QMessageBox::No);
                
            if (reply == QMessageBox::Yes) {
                // 使用已有H264文件
                m_selectedFilePath = targetH264Path;
                m_filePathEdit->setText(h264FileName);
                m_startButton->setEnabled(true);
                progress->setValue(100);
                QMessageBox::information(this, "文件准备完成", 
                    "将使用已有的H264文件进行推流。");
                return;
            }
        }
        
        // 复制MP4文件到exe同级目录
        progress->setValue(40);
        progress->setLabelText("正在复制MP4文件...");
        QCoreApplication::processEvents();
        
        // 如果目标位置已有同名MP4文件，先删除
        if (QFile::exists(targetMP4Path) && targetMP4Path != filePath) {
            QFile::remove(targetMP4Path);
        }
        
        // 如果源文件和目标不同，需要复制
        if (targetMP4Path != filePath) {
            if (!QFile::copy(filePath, targetMP4Path)) {
                progress->cancel();
                QMessageBox::critical(this, "文件复制失败", 
                    "无法复制MP4文件到程序目录。");
                return;
            }
        }
        
        // 开始转换
        progress->setValue(60);
        progress->setLabelText("正在转换MP4到H264，请稍候...");
        QCoreApplication::processEvents();
        
        // 使用FFmpegLoader转换文件
        if (FFmpegLoader::convertMP4ToH264(targetMP4Path, targetH264Path)) {
            progress->setValue(100);
            progress->setLabelText("转换完成！");
            QCoreApplication::processEvents();
            
            m_selectedFilePath = targetH264Path;
            m_filePathEdit->setText(h264FileName);
            m_startButton->setEnabled(true);
            
            QMessageBox::information(this, "转换成功", 
                "MP4文件已成功转换为H264格式，可以开始推流。");
        } else {
            progress->cancel();
            QMessageBox::critical(this, "转换失败", 
                "MP4转换失败，请检查是否已安装FFmpeg或选择H264格式文件。");
        }
    }
}

// 更新启动按钮状态
void MainWindow::updateStartButtonState()
{
    m_startButton->setEnabled(!m_filePathEdit->text().isEmpty());
} 