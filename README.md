# GB28181 视频平台

基于 GB28181 协议的视频平台客户端，支持多设备并发接入。

## 功能特点

- 支持多设备并发接入
- 支持设备注册、注销
- 支持实时视频流拉取
- 支持 H264 视频编码
- 支持 UDP/TCP 传输
- 支持配置文件管理
- 支持移动位置订阅测试
- 支持批量创建设备，进行压力测试
- 支持MP4文件转换为H264格式

## 版本说明
- MFC版：原始版本，基于MFC框架开发
- Qt版：新增版本，基于Qt框架开发，支持跨平台

## 更新记录
**2024-03-31：**
- 重构FFmpeg相关功能，统一管理到FFmpegLoader类
- 优化MP4到H264的转换流程，使用cmd.exe解决32位/64位兼容性问题
- 改进文件处理逻辑，支持MP4文件复制到exe同级目录
- 添加详细的进度反馈和错误提示
- 优化代码结构，提高可维护性

**2024-03-30：**
- 新增Qt版本，保持原有功能的同时，改进了界面和线程管理
- 修正Qt版本端口分配，与MFC版本保持一致（40000起始端口）
- 修复线程同步问题，解决了停止测试时可能出现的死锁问题
- 优化链接器选项，抑制PDB缺失警告(LNK4099)和默认库冲突警告(LNK4098)
- 重组项目结构，实现更清晰的代码组织和共享

**2021-08-05：**
- 支持mp4推国标流（视频文件不要有B帧，h264 profile最好不超过Main）

## 系统要求

- Windows 10 或更高版本
- Qt 5.12 或更高版本（Qt版）
- Visual Studio 2019 或更高版本
- C++11 或更高版本

## 编译说明

### 依赖库
- eXosip2 库
- osip2 库
- osipparser2 库
- libcares 库
- 对于Windows平台：ws2_32、Dnsapi等系统库

### MFC版
使用Visual Studio打开`GB28181_Stress_Tools.sln`进行编译

### Qt版
1. 安装Qt开发环境（推荐Qt 5.12以上）
2. 打开`GB28181_Stress_Tools_Qt.pro`
3. 配置编译环境
4. 运行qmake并编译项目
5. 编译完成后，可执行文件将生成在 `x64/Release` 目录下

## 配置说明

1. 配置文件 `config.xml` 位于可执行文件同目录下
2. 配置文件包含以下参数：
   - serverId: 服务器 SIP ID
   - serverIp: 服务器 IP 地址
   - serverPort: 服务器端口号
   - password: 认证密码
   - count: 设备数量

## 使用说明

1. 运行程序后，在配置区域填写服务器信息
2. 设置需要创建的设备数量
3. 点击"开始"按钮启动设备
4. 设备状态将在表格中实时显示

> 默认读取`bigbuckbunnynoB_480x272.h264`文件，如需更换视频源，重命名即可

## SIP交互流程
### 设备注册流程
1. 设备发起注册请求到SIP服务器
2. 服务器回复401，要求认证
3. 设备携带认证信息重新注册
4. 注册成功后，设备开始发送心跳（Keepalive）

### 流媒体交互流程
1. 服务器发送INVITE请求，携带SDP信息（目标IP、端口、协议等）
2. 设备解析SDP，获取推流目标信息
3. 设备生成响应SDP，分配本地端口（从40000开始）
4. 设备回复200 OK，携带响应SDP
5. 服务器发送ACK确认
6. 设备开始推流（通过UDP/TCP）

## 常见问题

- 确保服务器 SIP ID 至少 10 个字符
- 设备数量范围：1-10000
- 本地端口从 5060 开始递增
- 配置文件与可执行文件需在同一目录
- MFC和Qt版本端口分配一致性：两个版本都应从40000开始分配端口
- 推流失败检查：确保网络连接、防火墙设置、端口绑定权限
- H264文件要求：不应包含B帧，profile不超过Main

## Qt版新特性
- 使用Qt信号-槽机制替代MFC消息处理
- 设备操作在独立线程中运行，提高并发性能
- 改进的配置文件处理，使用XML格式
- 资源文件集成，简化部署
- 更好的日志输出和错误处理
- 采用现代化项目结构：src/ui、src/core、src/common实现代码分层
- 优化线程同步机制，防止死锁问题
- 添加特定链接器选项以提高编译稳定性

## 运行效果
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/mp4_preview.png)
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/page.png)
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/video_preview.png)

## 致谢
组包参考：[GB28181Android](https://github.com/zhoudd1/GB28181Android)

## 视频版权
视频来源于B站，如有侵权，请联系删除

## 许可证
[许可证类型]

## 联系方式
[联系方式]