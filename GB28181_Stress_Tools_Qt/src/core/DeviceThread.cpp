#include "DeviceThread.h"
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include "../common/LoadH264.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <pugixml.hpp>
#include "../common/FFmpegLoader.h"

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

void DeviceThread::setVideoPath(const QString& videoPath)
{
    m_videoPath = videoPath;
}

void DeviceThread::run()
{
    try {
        QString videoPath = m_videoPath;  // 使用已设置的路径
        
        qDebug() << "使用选择的视频文件:" << videoPath;
        
        // 检查文件是否存在
        if (!QFile::exists(videoPath)) {
            qDebug() << "错误: 选择的视频文件不存在:" << videoPath;
            emit deviceStatusUpdated(0, Message{STATUS_TYPE, "选择的视频文件不存在"});
            return;
        }
        
        // 获取文件信息
        QFileInfo fileInfo(videoPath);
        QString extension = fileInfo.suffix().toLower();
        
        // 如果是MP4文件但我们期望H264文件，尝试找到或生成H264文件
        if (extension == "mp4") {
            QString h264Path = fileInfo.absolutePath() + "/" + fileInfo.baseName() + ".h264";
            
            // 检查同名H264是否存在
            if (QFile::exists(h264Path)) {
                qDebug() << "找到与MP4同名的H264文件，直接使用:" << h264Path;
                videoPath = h264Path;
            } else {
                // 转换MP4到H264
                qDebug() << "未找到同名H264文件，尝试转换MP4";
                emit deviceStatusUpdated(0, Message{STATUS_TYPE, "正在转换MP4文件，请稍候..."});
                
                if (!FFmpegLoader::convertMP4ToH264(videoPath, h264Path)) {
                    qDebug() << "MP4转换失败，无法继续";
                    emit deviceStatusUpdated(0, Message{STATUS_TYPE, "MP4转换失败，请检查ffmpeg或选择H264文件"});
                    return;
                } else {
                    qDebug() << "MP4转换成功，使用转换后的H264文件:" << h264Path;
                    videoPath = h264Path;
                    emit deviceStatusUpdated(0, Message{STATUS_TYPE, "MP4转换成功，开始解析H264文件"});
                }
            }
        } else if (extension != "h264" && extension != "264") {
            // 如果既不是MP4也不是H264，则报错
            qDebug() << "错误: 不支持的文件格式:" << extension;
            emit deviceStatusUpdated(0, Message{STATUS_TYPE, "不支持的文件格式，请选择MP4或H264文件"});
            return;
        }
        
        // 加载视频文件
        qDebug() << "正在加载视频文件:" << videoPath;
        emit deviceStatusUpdated(0, Message{STATUS_TYPE, "正在解析视频文件..."});
        
        int loadResult = load(videoPath.toStdString().c_str());
        
        if (loadResult < 0) {
            qDebug() << "加载视频文件失败";
            emit deviceStatusUpdated(0, Message{STATUS_TYPE, "加载视频文件失败，请检查文件格式"});
            return;
        }
        
        extern std::vector<Nalu*> nalu_vector;
        qDebug() << "NALU向量大小:" << nalu_vector.size();
        
        if (nalu_vector.empty()) {
            qDebug() << "警告:NALU向量为空!";
            emit deviceStatusUpdated(0, Message{STATUS_TYPE, "视频帧解析失败，未找到有效的视频帧"});
            return;
        }
        
        emit deviceStatusUpdated(0, Message{STATUS_TYPE, "视频文件解析成功，已加载" + QString::number(nalu_vector.size()) + "帧"});
        NaluProvider naluProvider(&nalu_vector);
        
        // Create devices, assign ports, and start SIP clients
        for (int i = 0; i < m_deviceCount && m_isRunning; i++) {
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
                m_serverSipId.toStdString().c_str(), 
                m_serverIp.toStdString().c_str(), 
                m_serverPort, 
                m_password.toStdString().c_str(), 
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
    } catch (const std::exception& e) {
        qDebug() << "运行线程异常:" << e.what();
    }
} 