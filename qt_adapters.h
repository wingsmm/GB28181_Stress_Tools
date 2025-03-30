#pragma once

// 提供MFC相关功能的替代实现
#include <iostream>
#include <string>
#include <cstdio>
#include <QtCore/QDebug>

// 引入std命名空间中的常用对象
using std::cout;
using std::endl;

// 提供一个无操作的MFC函数替代
inline int get_port() {
    static int port = 40000;  // 从40000开始，与MFC版本保持一致
    qDebug() << "Getting port:" << port;
    return port++;
}

inline int get_sn() {
    static int sn = 1;
    qDebug() << "Getting SN:" << sn;
    return sn++;
} 