#include "DeviceThread.h"
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include "../common/LoadH264.h"

DeviceThread::DeviceThread(QObject *parent)
    : QThread(parent), m_isRunning(false), m_serverPort(5060), m_deviceCount(1)
{
}

DeviceThread::~DeviceThread()
{
    stop();
}

void DeviceThread::start(const QString& serverSipId, const QString& serverIp, int serverPort, 
                        const QString& password, int deviceCount)
{
    QMutexLocker locker(&m_mutex);
    m_serverSipId = serverSipId;
    m_serverIp = serverIp;
    m_serverPort = serverPort;
    m_password = password;
    m_deviceCount = deviceCount;
    m_isRunning = true;
    QThread::start();
}

void DeviceThread::stop()
{
    qDebug() << "正在停止设备线程...";
    
    // First set the running flag to false
    m_mutex.lock();
    m_isRunning = false;
    
    // Make a copy of the devices list to avoid issues if the list is modified during deregistration
    auto devicesCopy = m_devices;
    m_mutex.unlock();
    
    // Properly unregister each device using the public API
    qDebug() << "正在注销" << devicesCopy.size() << "个设备...";
    for (auto& device : devicesCopy) {
        if (device) {
            qDebug() << "正在停止设备" << device->list_index + 1;
            
            // Stop device using the public methods
            device->stopRunning();
            
            // If the device is pushing video, stop it
            if (device->isPushing()) {
                device->stopPushingStream();
                qDebug() << "正在停止设备" << device->list_index + 1 << "的视频推送";
            }
            
            // Stop heartbeat
            if (device->isHeartbeatRunning()) {
                device->stopHeartbeat();
                qDebug() << "正在停止设备" << device->list_index + 1 << "的心跳";
            }
            
            // Stop mobile position updates
            if (device->isMobilePositionRunning()) {
                device->stopMobilePosition();
                qDebug() << "正在停止设备" << device->list_index + 1 << "的位置更新";
            }
        }
    }
    
    // Wait a moment for devices to process stop signals
    msleep(500);
    
    // Clear devices list
    m_mutex.lock();
    m_devices.clear();
    m_mutex.unlock();
    
    qDebug() << "所有设备已注销，等待线程结束...";
    
    // Wait for thread to complete
    wait();
    
    qDebug() << "设备线程已成功停止";
}

void DeviceThread::run()
{
    m_mutex.lock();
    std::string serverSipId = m_serverSipId.toStdString();
    std::string serverIp = m_serverIp.toStdString();
    int serverPort = m_serverPort;
    std::string password = m_password.toStdString();
    int deviceCount = m_deviceCount;
    m_mutex.unlock();
    
    qDebug() << "设备线程已启动，参数如下：";
    qDebug() << "  服务器SIP ID:" << m_serverSipId;
    qDebug() << "  服务器IP:" << m_serverIp;
    qDebug() << "  服务器端口:" << serverPort;
    qDebug() << "  密码:" << m_password;
    qDebug() << "  设备数量:" << deviceCount;
    
    // Extract video file from resources to a temporary directory
    QString videoPath;
    QFile resourceFile(":/video.h264");
    if (resourceFile.exists()) {
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(false);
        if (tempFile.open()) {
            videoPath = tempFile.fileName();
            tempFile.close();
            
            // Copy resource to temporary file
            if (resourceFile.open(QIODevice::ReadOnly)) {
                QFile outFile(videoPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(resourceFile.readAll());
                    outFile.close();
                    qDebug() << "视频文件已复制到:" << videoPath;
                }
                resourceFile.close();
            }
        }
    } else {
        // Fallback to original path
        videoPath = "resources/video.h264";
        qDebug() << "使用默认视频文件路径:" << videoPath;
    }
    
    // Load H264 file
    qDebug() << "正在加载H264文件:" << videoPath;
    int loadResult = load(videoPath.toStdString().c_str());
    if (loadResult != 0) {
        qDebug() << "H264文件加载失败:" << loadResult;
    } else {
        qDebug() << "H264文件加载成功";
    }
    
    extern std::vector<Nalu*> nalu_vector;
    qDebug() << "NALU向量大小:" << nalu_vector.size();
    NaluProvider naluProvider(&nalu_vector);
    
    // Create devices, assign ports, and start SIP clients
    for (int i = 0; i < deviceCount && m_isRunning; i++) {
        // Generate device ID and channel ID
        char deviceId[128] = {0};
        sprintf(deviceId, "34020000001320000%03d", i + 1);
        
        char channelId[128] = {0};
        sprintf(channelId, "34020000001310000%03d", i + 1);
        
        int localPort = 5060 + i + 1;
        
        qDebug() << "正在创建设备" << i+1 << ":" << deviceId << "," << channelId << "," << "本地端口:" << localPort;
        
        // Use smart pointer to manage device object
        std::shared_ptr<Device> device = std::make_shared<Device>(
            deviceId, 
            channelId, 
            serverSipId.c_str(), 
            serverIp.c_str(), 
            serverPort, 
            password.c_str(), 
            &naluProvider
        );
        
        // Set device index and callback function
        device->list_index = i;
        device->set_callback([this, i](int /*index*/, Message msg) {
            qDebug() << "设备" << i+1 << "状态:" << msg.content;
            emit deviceStatusUpdated(i, msg);
        });
        
        // Store device for later cleanup
        m_mutex.lock();
        m_devices.push_back(device);
        m_mutex.unlock();
        
        // Notify main thread that device has been created
        qDebug() << "发送设备创建信号:" << i+1;
        emit deviceCreated(device);
        
        // Start SIP client
        qDebug() << "正在启动设备" << i+1 << "的SIP客户端";
        device->start_sip_client(localPort);
        
        // Delay to avoid port conflicts from creating too quickly
        qDebug() << "等待500毫秒...";
        msleep(500);
    }
    
    // Wait until thread is requested to stop
    qDebug() << "所有设备已创建，等待停止信号...";
    m_mutex.lock();
    while (m_isRunning) {
        m_mutex.unlock();
        msleep(100);
        m_mutex.lock();
    }
    m_mutex.unlock();
    qDebug() << "设备线程已结束";
} 