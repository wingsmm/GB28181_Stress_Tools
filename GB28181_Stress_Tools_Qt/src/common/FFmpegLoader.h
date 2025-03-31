#ifndef FFMPEGLOADER_H
#define FFMPEGLOADER_H

#include <QString>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QDateTime>
#include <QProgressDialog>

class FFmpegLoader {
private:
    static QString s_customFFmpegPath; // 存储自定义ffmpeg路径
    static bool s_initialized; // 标记是否已初始化

public:
    // 初始化并准备FFmpeg环境
    static bool initialize();
    
    // 设置自定义ffmpeg路径
    static void setCustomFFmpegPath(const QString& path);
    
    // 判断文件是否为MP4
    static bool isMP4File(const QString& filePath);
    
    // 转换MP4到H264
    static bool convertMP4ToH264(const QString& inputPath, const QString& outputPath);
    
    // 使用cmd.exe调用ffmpeg（解决32位/64位兼容性问题）
    static bool convertWithCmdExe(const QString& mp4FilePath, const QString& h264FilePath);
    
    // 查找FFmpeg可执行文件
    static QString findFFmpegExecutable();

    static bool checkFfmpegWorks(const QString& ffmpegPath);
    static bool prepareFFmpeg(QProgressDialog* progress);
    static bool copyFileWithProgress(const QString& source, const QString& destination);
};

#endif // FFMPEGLOADER_H

