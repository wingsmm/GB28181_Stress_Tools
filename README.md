# GB28181_Stress_Tools
[新项目，支持GB28181和1078](https://github.com/lkmio/lkm)

GB28181压力测试工具 - 包含MFC和Qt两个版本

## 版本说明
- MFC版：原始版本，基于MFC框架开发
- Qt版：新增版本，基于Qt框架开发，支持跨平台

## 更新
2024-03-30：
- 新增Qt版本，保持原有功能的同时，改进了界面和线程管理
- 修正Qt版本端口分配，与MFC版本保持一致（40000起始端口）

2021-08-05：支持mp4推国标流`视频文件不要有B帧，h264 profile最好不超过Main`

## 功能
- 支持移动位置订阅测试
- 支持TCP被动、UDP推流，内部实现h264组包ps，推rtp
- 支持批量创建设备，进行压力测试
- Qt版支持更现代的UI界面和更好的线程管理
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

### 常见问题
- MFC和Qt版本端口分配一致性：两个版本都应从40000开始分配端口
- 推流失败检查：确保网络连接、防火墙设置、端口绑定权限
- H264文件要求：不应包含B帧，profile不超过Main

## Qt版新特性
- 使用Qt信号-槽机制替代MFC消息处理
- 设备操作在独立线程中运行，提高并发性能
- 改进的配置文件处理，使用XML格式
- 资源文件集成，简化部署
- 更好的日志输出和错误处理

## 编译说明
### MFC版
使用Visual Studio打开`GB28181_Stress_Tools.sln`进行编译

### Qt版
1. 安装Qt开发环境（推荐Qt 5.12以上）
2. 打开`GB28181_Stress_Tools_Qt.pro`
3. 配置编译环境
4. 运行qmake并编译项目

## 依赖库
- eXosip
- osip2
- osipparser2
- libcares
- 对于Windows平台：ws2_32、Dnsapi等系统库

## 运行效果：
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/mp4_preview.png)
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/page.png)
![view](https://github.com/yangjiechina/GB28181_Stress_Tools/blob/master/GB28181_Stress_Tools/res/video_preview.png)

## Thanks
组包参考：[GB28181Android](https://github.com/zhoudd1/GB28181Android)

## 视频版权
视频来源于B站，如有侵权，请联系删除