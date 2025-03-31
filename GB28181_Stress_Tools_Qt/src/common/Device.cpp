#include "Device.h"
#include <sstream>
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <cstring>
#include <functional>
#include "pugixml.hpp"
#include "gb28181_header_maker.h"
#include <iomanip>
#include <ctime>
#include <QTimer>

void logSipError(const char* operation, int errorCode, int index) {
    std::cout << "SIP Error: Device" << index+1 << operation << "Error code:" << errorCode << std::endl;
}

void Device::mobile_position_task() {
    std::cout << "设备" << list_index+1 << "启动位置上报任务" << std::endl;
    
    while (is_running && is_mobile_position_running) {
        // 随机生成位置数据(模拟移动设备)
        double longitude = 118.0 + ((rand() % 1000) / 10000.0);
        double latitude = 31.0 + ((rand() % 1000) / 10000.0);
        
        // 构造XML消息
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
        ss << "<Notify>\r\n";
        ss << "<CmdType>MobilePosition</CmdType>\r\n";
        ss << "<SN>" << get_sn() << "</SN>\r\n";
        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
        ss << "<Time>" << get_current_time() << "</Time>\r\n";
        ss << "<Longitude>" << longitude << "</Longitude>\r\n";
        ss << "<Latitude>" << latitude << "</Latitude>\r\n";
        ss << "<Speed>5.5</Speed>\r\n";
        ss << "<Direction>120.5</Direction>\r\n";
        ss << "<Altitude>100.5</Altitude>\r\n";
        ss << "</Notify>\r\n";
        
        // 发送NOTIFY消息
        osip_message_t* notify = nullptr;
        eXosip_insubscription_build_notify(sip_context, mobile_postition_dialog_id, 
                                          EXOSIP_SUBCRSTATE_ACTIVE, 0, &notify);
        
        if (notify) {
            osip_message_set_content_type(notify, "Application/MANSCDP+xml");
            osip_message_set_body(notify, ss.str().c_str(), ss.str().length());
            eXosip_insubscription_send_request(sip_context, mobile_postition_dialog_id, notify);
            std::cout << "设备" << list_index+1 << "发送位置信息：经度 " << longitude << ", 纬度 " << latitude << std::endl;
        }
        
        // 等待下一个更新周期
        std::unique_lock<std::mutex> lck(_mobile_position_mutex);
        _mobile_postion_condition.wait_for(lck, std::chrono::seconds(5));
    }
    
    std::cout << "设备" << list_index+1 << "位置上报任务已结束" << std::endl;
}

void Device::create_heartbeat_task() {
    if (heartbeat_thread) {
        is_heartbeat_running = false;
        if (heartbeat_thread && heartbeat_thread->joinable()) {
            _heartbeat_condition.notify_one();
            heartbeat_thread->join();
        }
    }
    is_heartbeat_running = true;
    heartbeat_thread = std::make_shared<std::thread>(&Device::heartbeat_task, this);
}

void Device::create_push_stream_task() {
    if (push_stream_thread) {
        is_pushing = false;
        if (push_stream_thread->joinable()) {
            push_stream_thread->join();
        }
    }
    is_pushing = true;
    push_stream_thread = std::make_shared<std::thread>(&Device::push_task, this);
}

void Device::create_mobile_position_task() {
    std::cout << "设备" << list_index+1 << "创建位置上报任务" << std::endl;
    
    if (is_mobile_position_running) {
        std::cout << "设备" << list_index+1 << "位置上报任务已在运行中" << std::endl;
        return;
    }
    
    is_mobile_position_running = true;
    
    mobile_position_thread = std::make_shared<std::thread>(&Device::mobile_position_task, this);
    
    if (callback != nullptr) {
        callback(list_index, Message{ STATUS_TYPE, "位置上报已启动" });
    }
}

void Device::process_call(eXosip_event_t * evt) {
    std::cout << "Device" << list_index+1 << "Processing INVITE request" << std::endl;
    // 解析sdp
    osip_body_t *sdp_body = NULL;
    osip_message_get_body(evt->request, 0, &sdp_body);
    if (sdp_body != NULL) {
        std::cout << "Device" << list_index+1 << "Received SDP body:" << sdp_body->body << std::endl;
    }
    else {
        std::cout << "Device" << list_index+1 << "SDP body is empty" << std::endl;
        return;
    }
    sdp_message_t * sdp = NULL;

    if (OSIP_SUCCESS != sdp_message_init(&sdp)) {
        std::cout << "Device" << list_index+1 << "SDP message initialization failed" << std::endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "sdp_message_init failed" });
        }
        return;
    }
    if (OSIP_SUCCESS != sdp_message_parse(sdp, sdp_body->body)) {
        std::cout << "Device" << list_index+1 << "SDP message parsing failed" << std::endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "sdp_message_parse failed" });
        }
        return;
    }
    sdp_connection_t * connect = eXosip_get_video_connection(sdp);
    sdp_media_t * media = eXosip_get_video_media(sdp);
    target_ip = connect->c_addr;
    target_port = atoi(media->m_port);
    char * protocol = media->m_proto;
    std::cout << "Device" << list_index+1 << "Target info - IP:" << target_ip << "Port:" << target_port << "Protocol:" << (is_tcp ? "TCP" : "UDP") << std::endl;
    
    if (callback != nullptr) {
        char port[5];
        snprintf(port, 5, "%d", target_port);
        callback(list_index, Message{ PULL_STREAM_PROTOCOL_TYPE, is_tcp ? "TCP" : "UDP" });
        callback(list_index, Message{ PULL_STREAM_PORT_TYPE, port });
    }
    // 返回200_ok
    listen_port = get_port();
    char port[10];
    snprintf(port, 10, "%d", listen_port);
    std::cout << "Device" << list_index+1 << "Local listening port:" << listen_port << std::endl;
    
    if (callback != nullptr) {
        callback(list_index, Message{ PULL_STREAM_PORT_TYPE, port });
    }
    std::stringstream ss;
    ss << "v=0\r\n";
    ss << "o=" << videoChannelId << " 0 0 IN IP4 " << local_ip << "\r\n";
    ss << "s=Play\r\n";
    ss << "c=IN IP4 " << local_ip << "\r\n";
    ss << "t=0 0\r\n";
    if (is_tcp) {
        ss << "m=video " << listen_port << " TCP/RTP/AVP 96\r\n";
    }
    else {
        ss << "m=video " << listen_port << " RTP/AVP 96\r\n";
    }
    ss << "a=sendonly\r\n";
    ss << "a=rtpmap:96 PS/90000\r\n";
    ss << "y=4294967295\r\n";
    std::string sdp_str = ss.str();

    osip_message_t * message = evt->request;
    int status = eXosip_call_build_answer(sip_context, evt->tid, 200, &message);

    if (status != 0) {
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "Reply to invite failed" });
        }
        return;
    }

    osip_message_set_content_type(message, "APPLICATION/SDP");
    osip_message_set_body(message, sdp_str.c_str(), sdp_str.size());

    eXosip_call_send_answer(sip_context, evt->tid, 200, message);

    std::cout << "reply sdp " << sdp_str.c_str() << std::endl;
}

void Device::heartbeat_task() {
    while (is_running && register_success && is_heartbeat_running) {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\"?>\r\n";
        ss << "<Notify>\r\n";
        ss << "<CmdType>Keepalive</CmdType>\r\n";
        ss << "<SN>" << get_sn() << "</SN>\r\n";
        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
        ss << "<Status>OK</Status>\r\n";
        ss << "</Notify>\r\n";

        osip_message_t* request = create_request();
        if (request != NULL) {
            osip_message_set_content_type(request, "Application/MANSCDP+xml");
            osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
            send_request(request);
        }
        std::unique_lock<std::mutex> lck(_heartbeat_mutex);
        _heartbeat_condition.wait_for(lck, std::chrono::seconds(60));
    }
}

void Device::push_task() {
    std::cout << "Device" << list_index+1 << "Starting push stream task" << std::endl;
    udp_client = new UDPClient(is_tcp);

    int status = is_tcp ? udp_client->bind(local_ip, listen_port, target_ip, target_port) : udp_client->bind(local_ip, listen_port);

    if (0 != status) {
        std::cout << "Device" << list_index+1 << "Client port binding failed" << std::endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "Failed to bind local port or connect to remote" });
        }
        if (callId != -1 && dialogId != -1) {
            ExosipCtxLock lock(sip_context);
            eXosip_call_terminate(sip_context, callId, dialogId);
        }
        return;
    }
    std::cout << "Device" << list_index+1 << "Client binding successful - Local IP:" << local_ip << "Port:" << listen_port 
             << "Target IP:" << target_ip << "Port:" << target_port << std::endl;

    char ps_header[PS_HDR_LEN];
    char ps_system_header[SYS_HDR_LEN];
    char ps_map_header[PSM_HDR_LEN];
    char pes_header[PES_HDR_LEN];
    char rtp_header[RTP_HDR_LEN];

    int time_base = 90000;
    int fps = 25;

    int interval = time_base / fps;
    long pts = 0;

    char frame[1024 * 128];
    int single_packet_max_length = 1400;
    char rtp_packet[RTP_HDR_LEN + 1400];

    int ssrc = 0xffffffff;
    int rtp_seq = 0;

    extern std::vector<Nalu*> nalu_vector;
    size_t size = nalu_vector.size();
    std::cout << "Device" << list_index+1 << "Total NALU count:" << size << std::endl;

    while (is_pushing) {
        for (int i = 0; i < size; i++) {
            if (!is_pushing) {
                break;
            }
            Nalu *nalu = nalu_vector.at(i);
            
            // Enhanced NALU logging in MFC format
            std::cout << std::setw(5) << i << "| " << std::setw(8) << pts << "| " << std::setw(8) << "HIGH" << "| " << std::setw(6) << (nalu->type == NALU_TYPE_IDR ? "IDR" : "SLICE") << "| " << std::setw(8) << nalu->length << std::endl;

            NaluType type = nalu->type;
            int length = nalu->length;
            char * packet = nalu->packet;

            int index = 0;
            if (NALU_TYPE_IDR == type) {
                gb28181_make_ps_header(ps_header, pts);
                memcpy(frame, ps_header, PS_HDR_LEN);
                index += PS_HDR_LEN;

                gb28181_make_sys_header(ps_system_header, 0x3f);
                memcpy(frame + index, ps_system_header, SYS_HDR_LEN);
                index += SYS_HDR_LEN;

                gb28181_make_psm_header(ps_map_header);
                memcpy(frame + index, ps_map_header, PSM_HDR_LEN);
                index += PSM_HDR_LEN;
            }
            else {
                gb28181_make_ps_header(ps_header, pts);
                memcpy(frame, ps_header, PS_HDR_LEN);
                index += PS_HDR_LEN;
            }

            // 封装pes
            gb28181_make_pes_header(pes_header, 0xe0, length, pts, pts);
            memcpy(frame + index, pes_header, PES_HDR_LEN);
            index += PES_HDR_LEN;

            memcpy(frame + index, packet, length);
            index += length;

            // 拆分rtp
            int rtp_packet_count = ((index - 1) / single_packet_max_length) + 1;

            for (int i = 0; i < rtp_packet_count; i++) {
                gb28181_make_rtp_header(rtp_header, rtp_seq, pts, ssrc, i == (rtp_packet_count - 1));

                int writed_count = single_packet_max_length;
                if ((i + 1) * single_packet_max_length > index) {
                    writed_count = index - (i * single_packet_max_length);
                }
                
                int rtp_start_index = 0;
                unsigned short rtp_packet_length = RTP_HDR_LEN + writed_count;
                
                if (is_tcp) {
                    unsigned char packt_length_ary[2];
                    packt_length_ary[0] = (rtp_packet_length >> 8) & 0xff;
                    packt_length_ary[1] = rtp_packet_length & 0xff;
                    memcpy(rtp_packet, packt_length_ary, 2);
                    rtp_start_index = 2;
                }
                
                memcpy(rtp_packet + rtp_start_index, rtp_header, RTP_HDR_LEN);
                memcpy(rtp_packet + rtp_start_index + RTP_HDR_LEN, frame + (i * single_packet_max_length), writed_count);
                rtp_seq++;

                udp_client->send_packet(target_ip, target_port, rtp_packet, rtp_start_index + rtp_packet_length);
            }

            pts += interval;
            std::this_thread::sleep_for(std::chrono::milliseconds(38));
        }
    }
    
    if (udp_client != nullptr) {
        udp_client->release();
        delete udp_client;
        udp_client = nullptr;
    }
}

void Device::send_request(osip_message_t * request) {
    eXosip_lock(sip_context);
    eXosip_message_send_request(sip_context, request);
    eXosip_unlock(sip_context);
}

osip_message_t* Device::create_request() {
    osip_message_t* request = NULL;
    char fromSip[256] = { 0 };
    char toSip[256] = { 0 };

    if (!is_running) {
        return nullptr;
    }

    sprintf(fromSip, "<sip:%s@%s:%d>", deviceId, local_ip, local_port);
    sprintf(toSip, "<sip:%s@%s:%d>", server_sip_id, server_ip, server_port);

    int status = eXosip_message_build_request(sip_context,
        &request, "MESSAGE", toSip, fromSip, NULL);
    if (OSIP_SUCCESS != status) {
        std::cout << "build requests failed" << std::endl;
    }

    return request;
}

void Device::send_response(eXosip_event_t *evt, osip_message_t * message) {
    eXosip_lock(sip_context);
    eXosip_message_send_answer(sip_context, evt->tid, 200, message);
    eXosip_unlock(sip_context);
}

void Device::send_response_ok(eXosip_event_t *evt) {
    osip_message_t * message = evt->request;
    eXosip_message_build_answer(sip_context, evt->tid, 200, &message);
    send_response(evt, message);
}

void Device::process_request() {
    std::cout << "Device" << list_index+1 << "starting SIP message processing" << std::endl;
    eXosip_event_t *evt = NULL;
    
    // Add authentication info first
    if (strlen(password) > 0) {
        std::cout << "Device" << list_index+1 << "adding authentication info:" << deviceId << "password:" << password << std::endl;
        eXosip_lock(sip_context);
        eXosip_add_authentication_info(sip_context, deviceId, deviceId, password, "MD5", NULL);
        eXosip_unlock(sip_context);
    }
    
    while (is_running) {
        evt = eXosip_event_wait(sip_context, 0, 50);
        eXosip_lock(sip_context);
        eXosip_automatic_action(sip_context);
        eXosip_unlock(sip_context);
        if (evt == NULL) {
            continue;
        }
        
        std::cout << "Device" << list_index+1 << "received SIP event type:" << evt->type << std::endl;
        
        // Log event details
        if (evt->type == EXOSIP_MESSAGE_NEW && MSG_IS_MESSAGE(evt->request)) {
            osip_body_t *body = NULL;
            osip_message_get_body(evt->request, 0, &body);
            if (body != NULL) {
                std::cout << "Device" << list_index+1 << "received message body:" << body->body << std::endl;
            }
        }
        
        // Log registration events
        if (evt->type == EXOSIP_REGISTRATION_SUCCESS) {
            std::cout << "Device" << list_index+1 << "Registration successful" << std::endl;
        }
        
        // Log call events
        if (evt->type == EXOSIP_CALL_INVITE) {
            std::cout << "Device" << list_index+1 << "Received call invite" << std::endl;
        }
        
        switch (evt->type) {
        case EXOSIP_IN_SUBSCRIPTION_NEW: {
            std::cout << "设备" << list_index+1 << "接收到订阅请求" << std::endl;
            ExosipCtxLock lolck(sip_context);
            osip_message_t * answer = nullptr;
            if (OSIP_SUCCESS != eXosip_insubscription_build_answer(sip_context, evt->tid, 200, &answer)) {
                std::cout << "设备" << list_index+1 << "创建订阅回复失败" << std::endl;
                break;
            }
            eXosip_insubscription_send_answer(sip_context, evt->tid, 200, answer);
            mobile_postition_dialog_id = evt->did;
            create_mobile_position_task();
            std::cout << "设备" << list_index+1 << "响应订阅请求并开始位置上报" << std::endl;
            break;
        }

        case EXOSIP_MESSAGE_NEW: {
            std::cout << "Device" << list_index+1 << "received message request" << std::endl;
            if (MSG_IS_MESSAGE(evt->request)) {
                osip_body_t *body = NULL;
                osip_message_get_body(evt->request, 0, &body);
                if (body != NULL) {
                    std::cout << "Device" << list_index+1 << "message content:" << body->body << std::endl;
                }

                send_response_ok(evt);

                pugi::xml_document document;
                if (!document.load_buffer(body->body, strlen(body->body))) {
                    std::cout << "Device" << list_index+1 << "failed to parse XML" << std::endl;
                    break;
                }
                pugi::xml_node root_node = document.first_child();

                if (!root_node) {
                    std::cout << "Device" << list_index+1 << "failed to get root node" << std::endl;
                    break;
                }
                std::string root_name = root_node.name();
                if ("Query" == root_name) {
                    pugi::xml_node cmd_node = root_node.child("CmdType");

                    if (!cmd_node) {
                        std::cout << "Device" << list_index+1 << "failed to get CmdType node" << std::endl;
                        break;
                    }

                    pugi::xml_node sn_node = root_node.child("SN");
                    std::string cmd = cmd_node.child_value();
                    
                    std::cout << "Device" << list_index+1 << "processing command:" << cmd.c_str() << std::endl;
                    
                    if ("Catalog" == cmd) {
                        std::cout << "Device" << list_index+1 << "replied to directory query" << std::endl;
                        
                        std::stringstream ss;
                        ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                        ss << "<Response>\r\n";
                        ss << "<CmdType>Catalog</CmdType>\r\n";
                        ss << "<SN>" << sn_node.child_value() << "</SN>\r\n";
                        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
                        ss << "<SumNum>" << 1 << "</SumNum>\r\n";
                        ss << "<DeviceList Num=\"" << 1 << "\">\r\n";
                        ss << "<Item>\r\n";
                        ss << "<DeviceID>" << videoChannelId << "</DeviceID>\r\n";
                        ss << "<Name>IPC</Name>\r\n";
                        ss << "<Manufacturer>GB28181_Stress_Tools</Manufacturer>\r\n";
                        ss << "<Model>Qt</Model>\r\n";
                        ss << "<Owner>admin</Owner>\r\n";
                        ss << "<CivilCode>34020000</CivilCode>\r\n";
                        ss << "<Address>Local</Address>\r\n";
                        ss << "<Parental>0</Parental>\r\n";
                        ss << "<SafetyWay>0</SafetyWay>\r\n";
                        ss << "<RegisterWay>1</RegisterWay>\r\n";
                        ss << "<Secrecy>0</Secrecy>\r\n";
                        ss << "<Status>ON</Status>\r\n";
                        ss << "</Item>\r\n";
                        ss << "</DeviceList>\r\n";
                        ss << "</Response>\r\n";
                        
                        osip_message_t* request = create_request();
                        if (request != NULL) {
                            osip_message_set_content_type(request, "Application/MANSCDP+xml");
                            osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                            send_request(request);
                            std::cout << "Device" << list_index+1 << "sent directory reply successfully" << std::endl;
                        } else {
                            std::cout << "Device" << list_index+1 << "failed to create directory reply" << std::endl;
                        }
                    }
                    else if ("DeviceInfo" == cmd) {
                        std::cout << "Device" << list_index+1 << "replied to device info query" << std::endl;
                        
                        std::stringstream ss;
                        ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                        ss << "<Response>\r\n";
                        ss << "<CmdType>DeviceInfo</CmdType>\r\n";
                        ss << "<SN>" << sn_node.child_value() << "</SN>\r\n";
                        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
                        ss << "<DeviceName>IPC</DeviceName>\r\n";
                        ss << "<Manufacturer>GB28181_Stress_Tools</Manufacturer>\r\n";
                        ss << "<Model>Qt</Model>\r\n";
                        ss << "<Firmware>1.0.0</Firmware>\r\n";
                        ss << "<MaxCamera>1</MaxCamera>\r\n";
                        ss << "<MaxAlarm>0</MaxAlarm>\r\n";
                        ss << "<DeviceType>IPC</DeviceType>\r\n";
                        ss << "<Channel>1</Channel>\r\n";
                        ss << "</Response>\r\n";
                        
                        osip_message_t* request = create_request();
                        if (request != NULL) {
                            osip_message_set_content_type(request, "Application/MANSCDP+xml");
                            osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                            send_request(request);
                            std::cout << "Device" << list_index+1 << "sent device info reply successfully" << std::endl;
                        } else {
                            std::cout << "Device" << list_index+1 << "failed to create device info reply" << std::endl;
                        }
                    }
                    else if ("DeviceStatus" == cmd) {
                        std::cout << "Device" << list_index+1 << "replied to device status query" << std::endl;
                        
                        std::stringstream ss;
                        ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                        ss << "<Response>\r\n";
                        ss << "<CmdType>DeviceStatus</CmdType>\r\n";
                        ss << "<SN>" << sn_node.child_value() << "</SN>\r\n";
                        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
                        ss << "<Online>ONLINE</Online>\r\n";
                        ss << "<Status>OK</Status>\r\n";
                        ss << "<DeviceTime>" << get_current_time() << "</DeviceTime>\r\n";
                        ss << "<Alarmstatus>0</Alarmstatus>\r\n";
                        ss << "<Language>zh-CN</Language>\r\n";
                        ss << "<DeviceChannel>" << 1 << "</DeviceChannel>\r\n";
                        ss << "</Response>\r\n";
                        
                        osip_message_t* request = create_request();
                        if (request != NULL) {
                            osip_message_set_content_type(request, "Application/MANSCDP+xml");
                            osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                            send_request(request);
                            std::cout << "Device" << list_index+1 << "sent device status reply successfully" << std::endl;
                        } else {
                            std::cout << "Device" << list_index+1 << "failed to create device status reply" << std::endl;
                        }
                    }
                    else if ("DeviceControl" == cmd) {
                        std::cout << "Device" << list_index+1 << "received device control command" << std::endl;
                        
                        pugi::xml_node control_node = root_node.child("PTZCmd");
                        if (control_node) {
                            std::string ptz_cmd = control_node.child_value();
                            std::cout << "Device" << list_index+1 << "PTZ command:" << ptz_cmd.c_str() << std::endl;
                            
                            std::stringstream ss;
                            ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                            ss << "<Response>\r\n";
                            ss << "<CmdType>DeviceControl</CmdType>\r\n";
                            ss << "<SN>" << sn_node.child_value() << "</SN>\r\n";
                            ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
                            ss << "<Result>OK</Result>\r\n";
                            ss << "</Response>\r\n";
                            
                            osip_message_t* request = create_request();
                            if (request != NULL) {
                                osip_message_set_content_type(request, "Application/MANSCDP+xml");
                                osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                                send_request(request);
                                std::cout << "Device" << list_index+1 << "sent device control reply successfully" << std::endl;
                            } else {
                                std::cout << "Device" << list_index+1 << "failed to create device control reply" << std::endl;
                            }
                        }
                    }
                    else if ("PresetQuery" == cmd) {
                        std::cout << "Device" << list_index+1 << "replied to preset query" << std::endl;
                        
                        std::stringstream ss;
                        ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
                        ss << "<Response>\r\n";
                        ss << "<CmdType>PresetQuery</CmdType>\r\n";
                        ss << "<SN>" << sn_node.child_value() << "</SN>\r\n";
                        ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
                        ss << "<Num>1</Num>\r\n";
                        ss << "<PresetList>\r\n";
                        ss << "<Item>\r\n";
                        ss << "<PresetID>1</PresetID>\r\n";
                        ss << "<PresetName>Preset1</PresetName>\r\n";
                        ss << "</Item>\r\n";
                        ss << "</PresetList>\r\n";
                        ss << "</Response>\r\n";
                        
                        osip_message_t* request = create_request();
                        if (request != NULL) {
                            osip_message_set_content_type(request, "Application/MANSCDP+xml");
                            osip_message_set_body(request, ss.str().c_str(), strlen(ss.str().c_str()));
                            send_request(request);
                            std::cout << "Device" << list_index+1 << "sent preset query reply successfully" << std::endl;
                        } else {
                            std::cout << "Device" << list_index+1 << "failed to create preset query reply" << std::endl;
                        }
                    }
                    else if ("RecordInfo" == cmd) {
                        std::cout << "Device" << list_index+1 << "received recording query request, not processing" << std::endl;
                    }
                }
            }
            break;
        }

        case EXOSIP_REGISTRATION_SUCCESS: {
            std::cout << "Device" << list_index+1 << "registered successfully" << std::endl;
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Register success" });
            }
            register_success = true;
            create_heartbeat_task();
            break;
        }

        case EXOSIP_REGISTRATION_FAILURE: {
            std::cout << "Device" << list_index+1 << "registration failed, will retry" << std::endl;
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Register failed, retry..." });
            }
            
            register_success = false;
            if (evt->response == NULL) {
                return;
            }
            if (401 == evt->response->status_code) {
                if (callback != nullptr) {
                    callback(list_index, Message{ STATUS_TYPE, "Registration 401" });
                }
                osip_www_authenticate_t* www_authenticate_header;
                osip_message_get_www_authenticate(evt->response, 0, &www_authenticate_header);
                eXosip_add_authentication_info(sip_context, deviceId, deviceId, password, "MD5", www_authenticate_header->realm);
            }
            break;
        }

        case EXOSIP_CALL_INVITE: {
            std::cout << "Device" << list_index+1 << "received INVITE request" << std::endl;
            if (MSG_IS_INVITE(evt->request)) {
                if (callback != nullptr) {
                    callback(list_index, Message{ STATUS_TYPE, "Received INVITE request" });
                }
                process_call(evt);
                callId = evt->cid;
                dialogId = evt->did;
                create_push_stream_task();
            }
            break;
        }

        case EXOSIP_CALL_ACK: {
            std::cout << "Device" << list_index+1 << "received ACK request" << std::endl;
            //DO Nothing
            break;
        }

        case EXOSIP_CALL_CLOSED: {
            std::cout << "Device" << list_index+1 << "call closed" << std::endl;
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Call closed" });
            }
            is_pushing = false;
            break;
        }

        case EXOSIP_CALL_RELEASED: {
            std::cout << "Device" << list_index+1 << "call released" << std::endl;
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Call released" });
            }
            break;
        }

        default:
            std::cout << "Device" << list_index+1 << "received other event type:" << evt->type << std::endl;
            break;
        }

        eXosip_event_free(evt);
    }
    
    std::cout << "Device" << list_index+1 << "SIP message processing thread ended" << std::endl;
}

void Device::start_sip_client(int local_port) {
    std::cout << "Device" << list_index+1 << "starting SIP client, local port:" << local_port << std::endl;
    this->local_port = local_port;
    sip_context = eXosip_malloc();

    if (OSIP_SUCCESS != eXosip_init(sip_context)) {
        std::cout << "Device" << list_index+1 << "SIP initialization failed" << std::endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "exo_sip init failed" });
        }
        return;
    }

    if (OSIP_SUCCESS != eXosip_listen_addr(sip_context, IPPROTO_UDP, NULL, local_port, AF_INET, 0)) {
        std::cout << "Device" << list_index+1 << "SIP failed to bind port" << local_port << std::endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "sip bind port failed" });
        }
        eXosip_quit(sip_context);
        sip_context = nullptr;
        return;
    }
    is_running = true;

    std::cout << "Device" << list_index+1 << "creating SIP message processing thread" << std::endl;
    sip_thread = std::make_shared<std::thread>(&Device::process_request, this);

    char from_uri[128] = { 0 };
    char proxy_uri[128] = { 0 };
    char contact[128] = { 0 };

    eXosip_guess_localip(sip_context, AF_INET, local_ip, 128);
    sprintf(from_uri, "sip:%s@%s:%d", deviceId, local_ip, local_port);
    sprintf(contact, "sip:%s@%s:%d", deviceId, local_ip, local_port);
    sprintf(proxy_uri, "sip:%s@%s:%d", server_sip_id, server_ip, server_port);

    std::cout << "Device" << list_index+1 << "preparing registration: From:" << from_uri << "To:" << proxy_uri << "Contact:" << contact << "Local IP:" << local_ip << std::endl;

    eXosip_clear_authentication_info(sip_context);
    
    // Add authentication info
    if (strlen(password) > 0) {
        std::cout << "Device" << list_index+1 << "adding authentication info:" << deviceId << "password:" << password << std::endl;
        eXosip_add_authentication_info(sip_context, deviceId, deviceId, password, "MD5", NULL);
    }

    osip_message_t * register_message = NULL;
    int register_id = eXosip_register_build_initial_register(sip_context, from_uri, proxy_uri, contact, 3600, &register_message);
    if (register_message == NULL) {
        std::cout << "Device" << list_index+1 << "failed to create registration message, error code:" << register_id << std::endl;
        return;
    }
    
    eXosip_lock(sip_context);
    int result = eXosip_register_send_register(sip_context, register_id, register_message);
    eXosip_unlock(sip_context);
    std::cout << "Device" << list_index+1 << "registration request result:" << result << std::endl;
    
    if (callback != nullptr) {
        callback(list_index, Message{ STATUS_TYPE, "Sent registration message" });
    }
}

void Device::set_callback(std::function<void(int index, Message msg)> callback) {
    this->callback = std::move(callback);
}

Device::~Device() {
    is_running = false;
    if (sip_thread) {
        sip_thread->join();
        if (sip_context) {
            eXosip_quit(sip_context);
            sip_context = NULL;
        }
    }
    register_success = false;
    is_heartbeat_running = false;
    if (heartbeat_thread && heartbeat_thread->joinable()) {
        _heartbeat_condition.notify_one();
        heartbeat_thread->join();
    }
    is_mobile_position_running = false;
    if (mobile_position_thread && mobile_position_thread->joinable()) {
        _mobile_postion_condition.notify_one();
        mobile_position_thread->join();
    }

    is_pushing = false;
    if (push_stream_thread && push_stream_thread->joinable()) {
        push_stream_thread->join();
    }
    if (callback != nullptr) {
        callback(list_index, Message{ STATUS_TYPE, "Released device" });
    }
}

std::string Device::get_current_time() {
    time_t now = time(0);
    struct tm tm_now;
    char buf[80];
    
    localtime_s(&tm_now, &now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_now);
    
    return std::string(buf);
}
