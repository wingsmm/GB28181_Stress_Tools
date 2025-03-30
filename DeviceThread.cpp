#include "DeviceThread.h"
#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include "GB28181_Stress_Tools/LoadH264.h"

DeviceThread::DeviceThread(QObject *parent)
    : QThread(parent), m_isRunning(false), m_serverPort(5060), m_deviceCount(1)
{
}

DeviceThread::~DeviceThread()
{
    stop();
}

void DeviceThread::setParameters(const QString &serverSipId, const QString &serverIp, 
                                int serverPort, const QString &password, int deviceCount)
{
    QMutexLocker locker(&m_mutex);
    m_serverSipId = serverSipId;
    m_serverIp = serverIp;
    m_serverPort = serverPort;
    m_password = password;
    m_deviceCount = deviceCount;
}

void DeviceThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_isRunning = false;
}

void DeviceThread::run()
{
    m_mutex.lock();
    m_isRunning = true;
    
    std::string serverSipId = m_serverSipId.toStdString();
    std::string serverIp = m_serverIp.toStdString();
    int serverPort = m_serverPort;
    std::string password = m_password.toStdString();
    int deviceCount = m_deviceCount;
    
    qDebug() << "DeviceThread started, parameters:";
    qDebug() << "  ServerSipId:" << m_serverSipId;
    qDebug() << "  ServerIp:" << m_serverIp;
    qDebug() << "  ServerPort:" << serverPort;
    qDebug() << "  Password:" << m_password;
    qDebug() << "  DeviceCount:" << deviceCount;
    
    m_mutex.unlock();
    
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
                    qDebug() << "Video file copied to:" << videoPath;
                }
                resourceFile.close();
            }
        }
    } else {
        // Fallback to original path
        videoPath = "GB28181_Stress_Tools/bigbuckbunnynoB_480x272.h264";
        qDebug() << "Using default video file path:" << videoPath;
    }
    
    // Load H264 file
    qDebug() << "Loading H264 file:" << videoPath;
    int loadResult = load(videoPath.toStdString().c_str());
    if (loadResult != 0) {
        qDebug() << "H264 file loading failed:" << loadResult;
    } else {
        qDebug() << "H264 file loaded successfully";
    }
    
    extern std::vector<Nalu*> nalu_vector;
    qDebug() << "NALU vector size:" << nalu_vector.size();
    NaluProvider naluProvider(&nalu_vector);
    
    // Create devices, assign ports, and start SIP clients
    for (int i = 0; i < deviceCount && m_isRunning; i++) {
        // Generate device ID and channel ID
        char deviceId[128] = {0};
        sprintf(deviceId, "34020000001320000%03d", i + 1);
        
        char channelId[128] = {0};
        sprintf(channelId, "34020000001310000%03d", i + 1);
        
        int localPort = 5060 + i + 1;
        
        qDebug() << "Creating device" << i+1 << ":" << deviceId << "," << channelId << "," << "Local port:" << localPort;
        
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
            qDebug() << "Device" << i+1 << "status:" << msg.content;
            emit deviceStatus(i, msg);
        });
        
        // Notify main thread that device has been created
        qDebug() << "Sending device created signal:" << i+1;
        emit deviceCreated(device);
        
        // Start SIP client
        qDebug() << "Starting device" << i+1 << "SIP client";
        device->start_sip_client(localPort);
        
        // Delay to avoid port conflicts from creating too quickly
        qDebug() << "Delaying 500ms...";
        msleep(500);
    }
    
    // Wait until thread is requested to stop
    qDebug() << "All devices created, waiting for stop signal...";
    m_mutex.lock();
    while (m_isRunning) {
        m_mutex.unlock();
        msleep(100);
        m_mutex.lock();
    }
    m_mutex.unlock();
    qDebug() << "DeviceThread ended";
} 