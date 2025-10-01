#pragma once
#include <QString>

typedef enum {
	STATUS_TYPE,
	PULL_STREAM_PORT_TYPE,
	PULL_STREAM_PROTOCOL_TYPE,
} MESSAGE_TYPE;

struct Message {
	MESSAGE_TYPE type;
	QString content;
	
	// 默认构造函数
	Message() : type(STATUS_TYPE), content("") {}
	
	// 兼容const char*的构造函数
	Message(MESSAGE_TYPE t, const char* c) : type(t), content(c) {}
	
	// 支持QString的构造函数
	Message(MESSAGE_TYPE t, const QString& c) : type(t), content(c) {}
};
