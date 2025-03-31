#include "FFmpegLoader.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QCoreApplication>
#include <QDebug>

// 初始化静态成员
QString FFmpegLoader::s_customFFmpegPath;
bool FFmpegLoader::s_initialized = false;

bool FFmpegLoader::initialize()
{
    if (s_initialized) {
        return true; // 已初始化
    }
    
    // 尝试查找ffmpeg可执行文件
    QString ffmpegPath = findFFmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        qDebug() << "FFmpeg初始化失败：未找到ffmpeg.exe";
        return false;
    }
    
    qDebug() << "FFmpeg初始化：找到ffmpeg.exe: " << ffmpegPath;
    
    // 设置ffmpeg路径
    setCustomFFmpegPath(ffmpegPath);
    
    // 测试ffmpeg是否可用
    QString testCmd = QString("cmd.exe /c \"\"%1\" -version\"").arg(ffmpegPath);
    qDebug() << "FFmpeg初始化：测试ffmpeg可用性..." << testCmd;
    
    int testResult = system(testCmd.toStdString().c_str());
    if (testResult != 0) {
        qDebug() << "FFmpeg初始化失败：ffmpeg测试返回错误码" << testResult;
        return false;
    }
    
    qDebug() << "FFmpeg初始化成功";
    s_initialized = true;
    return true;
}

void FFmpegLoader::setCustomFFmpegPath(const QString& path)
{
    s_customFFmpegPath = path;
    qDebug() << "设置ffmpeg路径:" << path;
    
    // 检查路径是否有效
    QFileInfo fileInfo(path);
    if (fileInfo.exists() && fileInfo.isFile()) {
        qDebug() << "ffmpeg路径有效，文件大小:" << fileInfo.size() << "字节";
    } else {
        qDebug() << "警告: 设置的ffmpeg路径可能无效 - "
                 << "存在:" << fileInfo.exists()
                 << "是文件:" << fileInfo.isFile();
    }
}

bool FFmpegLoader::isMP4File(const QString& filePath)
{
    return filePath.toLower().endsWith(".mp4");
}

bool FFmpegLoader::convertMP4ToH264(const QString& inputPath, const QString& outputPath)
{
    // 确保FFmpeg已初始化
    if (!s_initialized && !initialize()) {
        qDebug() << "转换失败：FFmpeg未初始化";
        return false;
    }
    
    qDebug() << "开始转换MP4到H264:";
    qDebug() << "  输入:" << inputPath;
    qDebug() << "  输出:" << outputPath;
    
    if (!QFileInfo(inputPath).exists()) {
        qDebug() << "错误: MP4文件不存在:" << inputPath;
        return false;
    }
    
    if (QFileInfo(outputPath).exists()) {
        QFile::remove(outputPath);
        qDebug() << "移除已存在的输出文件:" << outputPath;
    }

    return convertWithCmdExe(inputPath, outputPath);
}

bool FFmpegLoader::convertWithCmdExe(const QString& mp4FilePath, const QString& h264FilePath)
{
    QString ffmpegPath = findFFmpegExecutable();
    if (ffmpegPath.isEmpty()) {
        qDebug() << "错误: 无法找到ffmpeg可执行文件";
        return false;
    }
    
    qDebug() << "使用cmd.exe调用32位ffmpeg...";
    
    // 使用cmd.exe启动ffmpeg，这样可以避免32位/64位兼容性问题
    QString command = QString("cmd.exe /c \"\"%1\" -i \"%2\" -c:v copy -bsf:v h264_mp4toannexb -an \"%3\"\"")
        .arg(ffmpegPath)
        .arg(mp4FilePath)
        .arg(h264FilePath);
    
    qDebug() << "执行命令:" << command;
    
    // 直接使用system()函数执行命令
    int result = system(command.toStdString().c_str());
    
    qDebug() << "命令执行完成，返回码:" << result;
    
    // 检查输出文件是否生成
    if (QFile::exists(h264FilePath) && QFileInfo(h264FilePath).size() > 0) {
        qDebug() << "H264文件已成功生成，大小:" << QFileInfo(h264FilePath).size() << "字节";
        return true;
    }
    
    // 尝试备用命令（更简单的命令）
    qDebug() << "标准命令失败，尝试简化命令...";
    QString simpleCommand = QString("cmd.exe /c \"\"%1\" -i \"%2\" \"%3\"\"")
        .arg(ffmpegPath)
        .arg(mp4FilePath)
        .arg(h264FilePath);
        
    qDebug() << "执行简化命令:" << simpleCommand;
    result = system(simpleCommand.toStdString().c_str());
    qDebug() << "简化命令执行完成，返回码:" << result;
    
    // 再次检查输出文件
    if (QFile::exists(h264FilePath) && QFileInfo(h264FilePath).size() > 0) {
        qDebug() << "使用简化命令生成H264文件成功，大小:" << QFileInfo(h264FilePath).size() << "字节";
        return true;
    }
    
    qDebug() << "所有转换尝试均失败，无法生成H264文件";
    return false;
}

QString FFmpegLoader::findFFmpegExecutable()
{
    // 优先使用自定义路径
    if (!s_customFFmpegPath.isEmpty()) {
        if (QFileInfo(s_customFFmpegPath).exists() && QFileInfo(s_customFFmpegPath).isFile()) {
            qDebug() << "使用自定义ffmpeg路径:" << s_customFFmpegPath;
            return s_customFFmpegPath;
        } else {
            qDebug() << "警告: 自定义ffmpeg路径无效:" << s_customFFmpegPath;
        }
    }
    
    // 尝试查找可执行文件同级目录的ffmpeg.exe
    QString appDir = QCoreApplication::applicationDirPath();
    QString executablePath = appDir + "/ffmpeg.exe";
    
    if (QFileInfo(executablePath).exists() && QFileInfo(executablePath).isFile()) {
        qDebug() << "在可执行文件同级目录找到ffmpeg.exe:" << executablePath;
        return executablePath;
    }
    
    // 如果同级目录没有找到，尝试系统PATH路径
    qDebug() << "在同级目录未找到ffmpeg.exe，尝试使用系统PATH";
    return "ffmpeg";
}

bool FFmpegLoader::checkFfmpegWorks(const QString& ffmpegPath)
{
    if (!QFile::exists(ffmpegPath)) {
        return false;
    }
    
    // 测试ffmpeg是否可用
    QString testCmd = QString("cmd.exe /c \"\"%1\" -version\"").arg(ffmpegPath);
    int testResult = system(testCmd.toStdString().c_str());
    
    return (testResult == 0);
}

bool FFmpegLoader::prepareFFmpeg(QProgressDialog* progress)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString ffmpegPath = appDir + "/ffmpeg.exe";
    
    // 检查ffmpeg是否可用
    bool ffmpegWorks = checkFfmpegWorks(ffmpegPath);
    
    if (ffmpegWorks) {
        progress->setValue(40);
        progress->setLabelText("FFmpeg已就绪");
        QCoreApplication::processEvents();
        return true;
    }
    
    // 如果ffmpeg.exe不存在或不可用，询问用户选择
    progress->cancel();
    
    QMessageBox::information(nullptr, "需要FFmpeg",
        "需要ffmpeg.exe来转换MP4文件。\n"
        "请选择ffmpeg.exe文件。\n\n"
        "ffmpeg.exe通常在ffmpeg的bin目录下。");
    
    // 用户选择ffmpeg.exe
    QString selectedFfmpegPath = QFileDialog::getOpenFileName(
        nullptr,
        "选择ffmpeg.exe",
        QDir::homePath(),
        "可执行文件 (ffmpeg.exe);;所有文件 (*.*)"
    );
    
    if (selectedFfmpegPath.isEmpty()) {
        return false; // 用户取消选择
    }
    
    // 复制ffmpeg.exe
    if (!copyFileWithProgress(selectedFfmpegPath, ffmpegPath)) {
        QMessageBox::critical(nullptr, "复制失败", "无法复制ffmpeg.exe文件。");
        return false;
    }
    
    // 测试复制后的ffmpeg是否可用
    if (!checkFfmpegWorks(ffmpegPath)) {
        QMessageBox::critical(nullptr, "FFmpeg测试失败", "复制的ffmpeg.exe无法正常工作。");
        return false;
    }
    
    QMessageBox::information(nullptr, "FFmpeg准备就绪", 
        "FFmpeg文件已准备就绪，可以开始转换MP4文件。");
    
    return true;
}

bool FFmpegLoader::copyFileWithProgress(const QString& source, const QString& destination)
{
    if (QFile::exists(destination)) {
        QFile::remove(destination);
    }
    
    return QFile::copy(source, destination);
} 