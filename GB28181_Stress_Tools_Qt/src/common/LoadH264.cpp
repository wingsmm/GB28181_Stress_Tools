#include "LoadH264.h"
#include "h264_parser.h"
#include "FFmpegLoader.h"
#include <QDebug>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QDir>
#include <QDateTime>

std::vector<Nalu*> nalu_vector;

char sps_data[128] = { 0 };
int sps_data_length = 0;

char pps_data[128] = { 0 };
int pps_data_length = 0;

void out_nalu(char* buffer, int size, NaluType naluType) {
	try {
		// 详细日志
		qDebug() << "解析NALU: 类型=" << naluType << "大小=" << size << "字节";
		
		// 处理SPS和PPS帧
		if (NALU_TYPE_SPS == naluType) {
			qDebug() << "处理SPS帧";
			memcpy(sps_data, buffer, size);
			sps_data_length = size;
			return;
		}

		if (NALU_TYPE_PPS == naluType) {
			qDebug() << "处理PPS帧";
			memcpy(pps_data, buffer, size);
			pps_data_length = size;
			return;
		}
		
		// 创建NAL单元
		Nalu* nalu = new Nalu;
		bool is_i_frame = (NALU_TYPE_IDR == naluType);
		
		if (is_i_frame) {
			qDebug() << "处理I帧，附加SPS/PPS头";
		}
		
		// 对于I帧，需要加上SPS和PPS头
		char* packet = nullptr;
		
		try {
			packet = (char*)malloc(is_i_frame ? (size + sps_data_length + pps_data_length) : size * sizeof(char));
			
			if (!packet) {
				qDebug() << "内存分配失败!";
				delete nalu;
				return;
			}
			
			if (is_i_frame) {
				memcpy(packet, sps_data, sps_data_length);
				memcpy(packet + sps_data_length, pps_data, pps_data_length);
				memcpy(packet + sps_data_length + pps_data_length, buffer, size);
				size += (sps_data_length + pps_data_length);
			} else {
				memcpy(packet, buffer, size);
			}
			
			nalu->packet = packet;
			nalu->length = size;
			nalu->type = naluType;
			
			nalu_vector.push_back(nalu);
		} catch (const std::exception& e) {
			qDebug() << "处理NALU异常:" << e.what();
			if (packet) free(packet);
			delete nalu;
		}
	} catch (const std::exception& e) {
		qDebug() << "NALU回调函数异常:" << e.what();
	}
}

int load(const char* path) {
	try {
		qDebug() << "开始加载视频文件:" << path;
		
		// 清空之前的NAL单元
		qDebug() << "清理之前的NALU向量，当前大小:" << nalu_vector.size();
		for (auto nalu : nalu_vector) {
			if (nalu->packet) {
				free(nalu->packet);
			}
			delete nalu;
		}
		nalu_vector.clear();
		
		// 重置SPS/PPS数据
		memset(sps_data, 0, sizeof(sps_data));
		sps_data_length = 0;
		memset(pps_data, 0, sizeof(pps_data));
		pps_data_length = 0;
		
		QString filePath(path);
		QFileInfo fileInfo(filePath);
		
		if (!fileInfo.exists()) {
			qDebug() << "错误: 文件不存在:" << filePath;
			return -1;
		}
		
		// 检查是否为MP4文件
		if (FFmpegLoader::isMP4File(filePath)) {
			qDebug() << "检测到MP4文件，开始转换处理:" << filePath;
			
			// 创建目标H264文件（使用同名但扩展名为h264的文件）
			QString h264FilePath = fileInfo.absolutePath() + "/" + fileInfo.baseName() + ".h264";
			qDebug() << "目标H264文件路径:" << h264FilePath;
			
			// 转换MP4到H264
			if (!FFmpegLoader::convertMP4ToH264(filePath, h264FilePath)) {
				qDebug() << "错误: MP4转换失败，无法继续";
				return -1;
			}
			
			// 加载转换后的H264文件
			qDebug() << "MP4转换成功，加载转换后的H264文件:" << h264FilePath;
			int result = simplest_h264_parser(h264FilePath.toStdString().c_str(), out_nalu);
			
			if (result < 0) {
				qDebug() << "错误: H264文件解析失败，请检查转换后的文件是否有效";
				return -1;
			}
			
			qDebug() << "H264解析完成，共生成" << nalu_vector.size() << "个NALU";
			
			// 检查解析结果
			if (nalu_vector.size() == 0) {
				qDebug() << "警告: 解析后未生成任何NALU数据，视频文件可能无效";
				return -1;
			}
			
			return result;
		} else {
			// 直接加载H264文件
			qDebug() << "加载H264文件:" << filePath;
			int result = simplest_h264_parser(path, out_nalu);
			
			if (result < 0) {
				qDebug() << "错误: H264文件解析失败";
				return -1;
			}
			
			qDebug() << "H264解析完成，共生成" << nalu_vector.size() << "个NALU";
			
			// 检查解析结果
			if (nalu_vector.size() == 0) {
				qDebug() << "警告: 解析后未生成任何NALU数据，视频文件可能无效";
				return -1;
			}
			
			return result;
		}
	} catch (const std::exception& e) {
		qDebug() << "加载视频文件异常:" << e.what();
		return -1;
	}
}