#include "qt_adapters.h"
#include "GB28181_Stress_Tools/Device.h"
#include <sstream>
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <cstring>
#include <functional>
#include <pugixml.hpp>
#include <QDebug>
#include "GB28181_Stress_Tools/gb28181_header_maker.h"

// 使用qt_adapters.h中的函数替代MFC中的功能
// 这是一个精简版本，只实现必要功能

// 提供详细的错误信息函数
void logSipError(const char* operation, int errorCode, int index) {
    qDebug() << "SIP错误: 设备" << index+1 << operation << "错误码:" << errorCode;
}

void Device::mobile_position_task() {
    while (is_running && is_mobile_position_running) {
        {
            osip_message_t * notify_message = NULL;
            ExosipCtxLock lock(sip_context);
            if (OSIP_SUCCESS != eXosip_insubscription_build_notify(sip_context, mobile_postition_dialog_id, EXOSIP_SUBCRSTATE_PENDING, EXOSIP_NOTIFY_PENDING, &notify_message)) {
                std::cout << "eXosip_insubscription_build_notify error" << std::endl;
                break;
            }
            std::stringstream ss;
            ss << "<?xml version=\"1.0\" encoding=\"GB2312\"?>\r\n";
            ss << "<Notify>\r\n";
            ss << "<DeviceID>" << deviceId << "</DeviceID>\r\n";
            ss << "<CmdType>MobilePosition</CmdType>\r\n";
            ss << "<SN>" << mobile_position_sn << "</SN>\r\n";
            ss << "<Time>" << "</Time>\r\n";
            ss << "<Longitude>" << "116.405994" << "</Longitude>\r\n";
            ss << "<Latitude>" << "39.914492" << "</Latitude>\r\n";
            ss << "<Speed>0.0</Speed>\r\n";
            ss << "<Direction>0.0</Direction>\r\n";
            ss << "<Altitude>0.0</Altitude>\r\n";
            ss << "</Notify>\r\n";
            osip_message_set_content_type(notify_message, "Application/MANSCDP+xml");
            osip_message_set_body(notify_message, ss.str().c_str(), strlen(ss.str().c_str()));
            eXosip_insubscription_send_request(sip_context, mobile_postition_dialog_id, notify_message);
        }
        std::unique_lock<std::mutex> lck(_mobile_position_mutex);
        _mobile_postion_condition.wait_for(lck, std::chrono::seconds(5));
    }
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
    if (mobile_position_thread) {
        is_mobile_position_running = false;
        if (mobile_position_thread->joinable()) {
            _mobile_postion_condition.notify_one();
            mobile_position_thread->join();
        }
    }
    is_mobile_position_running = true;
    mobile_position_thread = std::make_shared<std::thread>(&Device::mobile_position_task, this);
}

void Device::process_call(eXosip_event_t * evt) {
    // 解析sdp
    osip_body_t *sdp_body = NULL;
    osip_message_get_body(evt->request, 0, &sdp_body);
    if (sdp_body != NULL) {
        printf("request >>> %s", sdp_body->body);
    }
    else {
        cout << "sdp error" << endl;
        return;
    }
    sdp_message_t * sdp = NULL;

    if (OSIP_SUCCESS != sdp_message_init(&sdp)) {
        cout << "sdp_message_init failed" << endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "sdp_message_init failed" });
        }
        return;
    }
    if (OSIP_SUCCESS != sdp_message_parse(sdp, sdp_body->body)) {
        cout << "sdp_message_parse failed" << endl;
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
    is_tcp = strstr(protocol, "TCP");
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

    cout << "reply sdp " << sdp_str.c_str() << endl;
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
    udp_client = new UDPClient(is_tcp);

    int status = is_tcp ? udp_client->bind(local_ip, listen_port, target_ip, target_port) : udp_client->bind(local_ip, listen_port);

    if (0 != status) {
        cout << "client bind port failed" << endl;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "Failed to bind local port or connect to remote" });
        }
        if (callId != -1 && dialogId != -1) {
            ExosipCtxLock lock(sip_context);
            eXosip_call_terminate(sip_context, callId, dialogId);
        }
        return;
    }

    char ps_header[PS_HDR_LEN];
    char ps_system_header[SYS_HDR_LEN];
    char ps_map_header[PSM_HDR_LEN];
    char pes_header[PES_HDR_LEN];
    char rtp_header[RTP_HDR_LEN];

    int time_base = 90000;
    int fps = 25;
    int send_packet_interval = 1000 / fps;

    int interval = time_base / fps;
    long pts = 0;

    char frame[1024 * 128];
    int single_packet_max_length = 1400;
    char rtp_packet[RTP_HDR_LEN + 1400];

    int ssrc = 0xffffffff;
    int rtp_seq = 0;

    extern std::vector<Nalu*> nalu_vector;
    size_t size = nalu_vector.size();

    while (is_pushing) {
        for (int i = 0; i < size; i++) {
            if (!is_pushing) {
                break;
            }
            Nalu *nalu = nalu_vector.at(i);

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
    qDebug() << "Device" << list_index+1 << "starting SIP message processing";
    eXosip_event_t *evt = NULL;
    
    // Add authentication info first
    if (strlen(password) > 0) {
        qDebug() << "Device" << list_index+1 << "adding authentication info:" << deviceId << "password:" << password;
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
        
        qDebug() << "Device" << list_index+1 << "received SIP event type:" << evt->type;

        switch (evt->type) {
        case EXOSIP_IN_SUBSCRIPTION_NEW: {
            qDebug() << "Device" << list_index+1 << "received subscription request";
            ExosipCtxLock lolck(sip_context);
            osip_message_t * answer = NULL;
            if (OSIP_SUCCESS != eXosip_insubscription_build_answer(sip_context, evt->tid, 200, &answer)) {
                qDebug() << "Device" << list_index+1 << "failed to create subscription reply";
                break;
            }
            eXosip_insubscription_send_answer(sip_context, evt->tid, 200, answer);
            mobile_postition_dialog_id = evt->did;
            create_mobile_position_task();
            qDebug() << "Device" << list_index+1 << "replied to subscription request and created position task";
            break;
        }

        case EXOSIP_MESSAGE_NEW: {
            qDebug() << "Device" << list_index+1 << "received message request";
            if (MSG_IS_MESSAGE(evt->request)) {
                osip_body_t *body = NULL;
                osip_message_get_body(evt->request, 0, &body);
                if (body != NULL) {
                    qDebug() << "Device" << list_index+1 << "message content:" << body->body;
                }

                send_response_ok(evt);

                pugi::xml_document document;
                if (!document.load_buffer(body->body, strlen(body->body))) {
                    qDebug() << "Device" << list_index+1 << "failed to parse XML";
                    break;
                }
                pugi::xml_node root_node = document.first_child();

                if (!root_node) {
                    qDebug() << "Device" << list_index+1 << "failed to get root node";
                    break;
                }
                std::string root_name = root_node.name();
                if ("Query" == root_name) {
                    pugi::xml_node cmd_node = root_node.child("CmdType");

                    if (!cmd_node) {
                        qDebug() << "Device" << list_index+1 << "failed to get CmdType node";
                        break;
                    }

                    pugi::xml_node sn_node = root_node.child("SN");
                    std::string cmd = cmd_node.child_value();
                    
                    qDebug() << "Device" << list_index+1 << "processing command:" << cmd.c_str();
                    
                    if ("Catalog" == cmd) {
                        qDebug() << "Device" << list_index+1 << "replied to directory query";
                        
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
                            qDebug() << "Device" << list_index+1 << "sent directory reply successfully";
                        } else {
                            qDebug() << "Device" << list_index+1 << "failed to create directory reply";
                        }
                    }
                    else if ("RecordInfo" == cmd) {
                        //processRecordInfo(root_note);
                        qDebug() << "Device" << list_index+1 << "received recording query request, not processing";
                    }
                }
            }
            break;
        }

        case EXOSIP_REGISTRATION_SUCCESS: {
            qDebug() << "Device" << list_index+1 << "registered successfully";
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Register success" });
            }
            register_success = true;
            create_heartbeat_task();
            break;
        }

        case EXOSIP_REGISTRATION_FAILURE: {
            qDebug() << "Device" << list_index+1 << "registration failed, will retry";
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Register failed, retry..." });
            }
            
            // Try sending registration again
            char from_uri[128] = { 0 };
            char proxy_uri[128] = { 0 };
            char contact[128] = { 0 };
            
            sprintf(from_uri, "sip:%s@%s:%d", deviceId, local_ip, local_port);
            sprintf(contact, "sip:%s@%s:%d", deviceId, local_ip, local_port);
            sprintf(proxy_uri, "sip:%s@%s:%d", server_sip_id, server_ip, server_port);
            
            qDebug() << "Device" << list_index+1 << "resending registration request: From:" << from_uri << "To:" << proxy_uri << "Contact:" << contact;
                     
            eXosip_lock(sip_context);

            // Ensure adding authentication info
            eXosip_clear_authentication_info(sip_context);
            eXosip_add_authentication_info(sip_context, deviceId, deviceId, password, "MD5", NULL);
            
            osip_message_t * register_message = NULL;
            int register_id = eXosip_register_build_initial_register(sip_context, from_uri, proxy_uri, contact, 3600, &register_message);
            if (register_message == NULL) {
                qDebug() << "Device" << list_index+1 << "failed to create registration message";
                return;
            }
            
            int result = eXosip_register_send_register(sip_context, register_id, register_message);
            eXosip_unlock(sip_context);
            qDebug() << "Device" << list_index+1 << "resending registration result:" << result;
            break;
        }

        case EXOSIP_CALL_INVITE: {
            qDebug() << "Device" << list_index+1 << "received INVITE request";
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
            qDebug() << "Device" << list_index+1 << "received ACK request";
            //DO Nothing
            break;
        }

        case EXOSIP_CALL_CLOSED: {
            qDebug() << "Device" << list_index+1 << "call closed";
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Call closed" });
            }
            is_pushing = false;
            break;
        }

        case EXOSIP_CALL_RELEASED: {
            qDebug() << "Device" << list_index+1 << "call released";
            if (callback != nullptr) {
                callback(list_index, Message{ STATUS_TYPE, "Call released" });
            }
            break;
        }

        default:
            qDebug() << "Device" << list_index+1 << "received other event type:" << evt->type;
            break;
        }

        eXosip_event_free(evt);
    }
    
    qDebug() << "Device" << list_index+1 << "SIP message processing thread ended";
}

void Device::start_sip_client(int local_port) {
    qDebug() << "Device" << list_index+1 << "starting SIP client, local port:" << local_port;
    this->local_port = local_port;
    sip_context = eXosip_malloc();

    if (OSIP_SUCCESS != eXosip_init(sip_context)) {
        qDebug() << "Device" << list_index+1 << "SIP initialization failed";
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "exo_sip init failed" });
        }
        return;
    }

    if (OSIP_SUCCESS != eXosip_listen_addr(sip_context, IPPROTO_UDP, NULL, local_port, AF_INET, 0)) {
        qDebug() << "Device" << list_index+1 << "SIP failed to bind port" << local_port;
        if (callback != nullptr) {
            callback(list_index, Message{ STATUS_TYPE, "sip bind port failed" });
        }
        eXosip_quit(sip_context);
        sip_context = nullptr;
        return;
    }
    is_running = true;

    qDebug() << "Device" << list_index+1 << "creating SIP message processing thread";
    sip_thread = std::make_shared<std::thread>(&Device::process_request, this);

    char from_uri[128] = { 0 };
    char proxy_uri[128] = { 0 };
    char contact[128] = { 0 };

    eXosip_guess_localip(sip_context, AF_INET, local_ip, 128);
    sprintf(from_uri, "sip:%s@%s:%d", deviceId, local_ip, local_port);
    sprintf(contact, "sip:%s@%s:%d", deviceId, local_ip, local_port);
    sprintf(proxy_uri, "sip:%s@%s:%d", server_sip_id, server_ip, server_port);

    qDebug() << "Device" << list_index+1 << "preparing registration: From:" << from_uri << "To:" << proxy_uri << "Contact:" << contact << "Local IP:" << local_ip;

    eXosip_clear_authentication_info(sip_context);
    
    // Add authentication info
    if (strlen(password) > 0) {
        qDebug() << "Device" << list_index+1 << "adding authentication info:" << deviceId << "password:" << password;
        eXosip_add_authentication_info(sip_context, deviceId, deviceId, password, "MD5", NULL);
    }

    osip_message_t * register_message = NULL;
    int register_id = eXosip_register_build_initial_register(sip_context, from_uri, proxy_uri, contact, 3600, &register_message);
    if (register_message == NULL) {
        qDebug() << "Device" << list_index+1 << "failed to create registration message, error code:" << register_id;
        return;
    }
    
    eXosip_lock(sip_context);
    int result = eXosip_register_send_register(sip_context, register_id, register_message);
    eXosip_unlock(sip_context);
    qDebug() << "Device" << list_index+1 << "registration request result:" << result;
    
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