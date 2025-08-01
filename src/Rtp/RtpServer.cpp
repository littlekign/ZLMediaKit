﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "Util/uv_errno.h"
#include "RtpServer.h"
#include "RtpProcess.h"
#include "Rtcp/RtcpContext.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

RtpServer::~RtpServer() {
    if (_on_cleanup) {
        _on_cleanup();
    }
}

class RtcpHelper: public std::enable_shared_from_this<RtcpHelper> {
public:
    using Ptr = std::shared_ptr<RtcpHelper>;

    RtcpHelper(Socket::Ptr rtcp_sock, MediaTuple tuple) {
        _rtcp_sock = std::move(rtcp_sock);
        _tuple = std::move(tuple);
    }

    void setRtpServerInfo(uint16_t local_port, RtpServer::TcpMode mode, bool re_use_port, uint32_t ssrc, int only_track) {
        _ssrc = ssrc;
        _process = RtpProcess::createProcess(_tuple);
        _process->setOnlyTrack((RtpProcess::OnlyTrack)only_track);

        _timeout_cb = [=]() mutable {
            NOTICE_EMIT(BroadcastRtpServerTimeoutArgs, Broadcast::kBroadcastRtpServerTimeout, local_port, _tuple, (int)mode, re_use_port, ssrc);
        };

        weak_ptr<RtcpHelper> weak_self = shared_from_this();
        _process->setOnDetach([weak_self](const SockException &ex) {
            if (auto strong_self = weak_self.lock()) {
                if (strong_self->_on_detach) {
                    strong_self->_on_detach(ex);
                }
                if (ex.getErrCode() == Err_timeout) {
                    strong_self->_timeout_cb();
                }
            }
        });
    }

    void setOnDetach(RtpProcess::onDetachCB cb) { _on_detach = std::move(cb); }

    RtpProcess::Ptr getProcess() const { return _process; }

    void onRecvRtp(const Socket::Ptr &sock, const Buffer::Ptr &buf, struct sockaddr *addr) {
        try {
            _process->inputRtp(true, sock, buf->data(), buf->size(), addr);
        } catch (std::exception &ex) {
            _process->onDetach(SockException(Err_shutdown, ex.what()));
            return;
        }
        // 统计rtp接受情况，用于发送rr包  [AUTO-TRANSLATED:bd2fbe7e]
        // Count RTP reception status, used to send RR packets
        auto header = (RtpHeader *)buf->data();
        sendRtcp(ntohl(header->ssrc), addr);
    }

    void startRtcp() {
        weak_ptr<RtcpHelper> weak_self = shared_from_this();
        _rtcp_sock->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
            // 用于接受rtcp打洞包  [AUTO-TRANSLATED:d75d9d87]
            // Used to receive RTCP hole punching packets
            auto strong_self = weak_self.lock();
            if (!strong_self || !strong_self->_process) {
                return;
            }
            if (!strong_self->_rtcp_addr) {
                // 只设置一次rtcp对端端口  [AUTO-TRANSLATED:fdc4eb4e]
                // Set the RTCP peer port only once
                strong_self->_rtcp_addr = std::make_shared<struct sockaddr_storage>();
                memcpy(strong_self->_rtcp_addr.get(), addr, addr_len);
            }
            auto rtcps = RtcpHeader::loadFromBytes(buf->data(), buf->size());
            for (auto &rtcp : rtcps) {
                strong_self->_process->onRtcp(rtcp);
            }
            // 收到sr rtcp后驱动返回rr rtcp  [AUTO-TRANSLATED:d7373077]
            // After receiving SR RTCP, the driver returns RR RTCP
            strong_self->sendRtcp(strong_self->_ssrc, (struct sockaddr *)(strong_self->_rtcp_addr.get()));
        });
    }

private:
    void sendRtcp(uint32_t rtp_ssrc, struct sockaddr *addr) {
        // 每5秒发送一次rtcp  [AUTO-TRANSLATED:3c9bcb7b]
        // Send RTCP every 5 seconds
        if (_ticker.elapsedTime() < 5000) {
            return;
        }
        _ticker.resetTime();

        auto rtcp_addr = (struct sockaddr *)_rtcp_addr.get();
        if (!rtcp_addr) {
            // 默认的，rtcp端口为rtp端口+1  [AUTO-TRANSLATED:273d164d]
            // By default, the RTCP port is the RTP port + 1
            switch (addr->sa_family) {
                case AF_INET: ((sockaddr_in *)addr)->sin_port = htons(ntohs(((sockaddr_in *)addr)->sin_port) + 1); break;
                case AF_INET6: ((sockaddr_in6 *)addr)->sin6_port = htons(ntohs(((sockaddr_in6 *)addr)->sin6_port) + 1); break;
            }
            // 未收到rtcp打洞包时，采用默认的rtcp端口  [AUTO-TRANSLATED:d99b12b8]
            // When no RTCP hole punching packet is received, the default RTCP port is used
            rtcp_addr = addr;
        }
        _rtcp_sock->send(_process->createRtcpRR(rtp_ssrc + 1, rtp_ssrc), rtcp_addr);
    }

private:
    uint32_t _ssrc = 0;
    std::function<void()> _timeout_cb;
    Ticker _ticker;
    Socket::Ptr _rtcp_sock;
    RtpProcess::Ptr _process;
    MediaTuple _tuple;
    RtpProcess::onDetachCB _on_detach;
    std::shared_ptr<struct sockaddr_storage> _rtcp_addr;
};

void RtpServer::start(uint16_t local_port, const char *local_ip, const MediaTuple &tuple, TcpMode tcp_mode, bool re_use_port, uint32_t ssrc, int only_track, bool multiplex) {
    // 创建udp服务器  [AUTO-TRANSLATED:99619428]
    // Create UDP server
    auto poller = EventPollerPool::Instance().getPoller();
    Socket::Ptr rtp_socket = Socket::createSocket(poller, true);
    Socket::Ptr rtcp_socket = Socket::createSocket(poller, true);
    if (local_port == 0) {
        // 随机端口，rtp端口采用偶数  [AUTO-TRANSLATED:3664eaf5]
        // Random port, RTP port uses even numbers
        auto pair = std::make_pair(rtp_socket, rtcp_socket);
        makeSockPair(pair, local_ip, re_use_port);
        local_port = rtp_socket->get_local_port();
    } else if (!rtp_socket->bindUdpSock(local_port, local_ip, re_use_port)) {
        // 用户指定端口  [AUTO-TRANSLATED:1328393b]
        // User-specified port
        throw std::runtime_error(StrPrinter << "创建rtp端口 " << local_ip << ":" << local_port << " 失败:" << get_uv_errmsg(true));
    } else if (!rtcp_socket->bindUdpSock(local_port + 1, local_ip, re_use_port)) {
        // rtcp端口  [AUTO-TRANSLATED:00cf932e]
        // RTCP port
        throw std::runtime_error(StrPrinter << "创建rtcp端口 " << local_ip << ":" << local_port + 1 << " 失败:" << get_uv_errmsg(true));
    }

    // 设置udp socket读缓存  [AUTO-TRANSLATED:3bf101d7]
    // Set UDP socket read cache
    GET_CONFIG(int, udpRecvSocketBuffer, RtpProxy::kUdpRecvSocketBuffer);
    SockUtil::setRecvBuf(rtp_socket->rawFD(), udpRecvSocketBuffer);

    // 创建udp服务器  [AUTO-TRANSLATED:99619428]
    // Create UDP server
    UdpServer::Ptr udp_server;
    RtcpHelper::Ptr helper;
    // 增加了多路复用判断，如果多路复用为true，就走else逻辑，同时保留了原来stream_id为空走else逻辑  [AUTO-TRANSLATED:114690b1]
    // Added multiplexing judgment. If multiplexing is true, then go to the else logic, while retaining the original stream_id is empty to go to the else logic
    if (!tuple.stream.empty() && !multiplex) {
        // 指定了流id，那么一个端口一个流(不管是否包含多个ssrc的多个流，绑定rtp源后，会筛选掉ip端口不匹配的流)  [AUTO-TRANSLATED:3aeb611e]
        // If a stream ID is specified, then one port is one stream (regardless of whether it contains multiple streams with multiple SSRCs, after binding the RTP source, streams that do not match the IP port will be filtered out)
        helper = std::make_shared<RtcpHelper>(std::move(rtcp_socket), tuple);
        helper->startRtcp();
        helper->setRtpServerInfo(local_port, tcp_mode, re_use_port, ssrc, only_track);
        bool bind_peer_addr = false;
        auto ssrc_ptr = std::make_shared<uint32_t>(ssrc);
        _ssrc = ssrc_ptr;
        rtp_socket->setOnRead([rtp_socket, helper, ssrc_ptr, bind_peer_addr](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) mutable {
            RtpHeader *header = (RtpHeader *)buf->data();
            auto rtp_ssrc = ntohl(header->ssrc);
            auto ssrc = *ssrc_ptr;
            if (ssrc && rtp_ssrc != ssrc) {
                WarnL << "ssrc mismatched, rtp dropped: " << rtp_ssrc << " != " << ssrc;
            } else {
                if (!bind_peer_addr) {
                    // 绑定对方ip+端口，防止多个设备或一个设备多次推流从而日志报ssrc不匹配问题  [AUTO-TRANSLATED:f27dd373]
                    // Bind the peer IP + port to prevent multiple devices or one device from pushing multiple streams, resulting in log reports of mismatched SSRCs
                    bind_peer_addr = true;
                    rtp_socket->bindPeerAddr(addr, addr_len);
                }
                helper->onRecvRtp(rtp_socket, buf, addr);
            }
        });
    } else {
        // 单端口多线程接收多个流，根据ssrc区分流  [AUTO-TRANSLATED:e11c3ca8]
        // Single-port multi-threaded reception of multiple streams, distinguishing streams based on SSRC
        udp_server = std::make_shared<UdpServer>();
        (*udp_server)[RtpSession::kOnlyTrack] = only_track;
        (*udp_server)[RtpSession::kUdpRecvBuffer] = udpRecvSocketBuffer;
        (*udp_server)[RtpSession::kVhost] = tuple.vhost;
        (*udp_server)[RtpSession::kApp] = tuple.app;
        udp_server->start<RtpSession>(local_port, local_ip);
        rtp_socket = nullptr;
    }

    TcpServer::Ptr tcp_server;
    if (tcp_mode == PASSIVE || tcp_mode == ACTIVE) {
        auto processor = helper ? helper->getProcess() : nullptr;
        // 如果共享同一个processor对象，那么tcp server声明为单线程模式确保线程安全  [AUTO-TRANSLATED:68bdd877]
        // If the same processor object is shared, declare the TCP server in single-threaded mode to ensure thread safety.
        tcp_server = std::make_shared<TcpServer>(processor ? poller : nullptr);
        (*tcp_server)[RtpSession::kVhost] = tuple.vhost;
        (*tcp_server)[RtpSession::kApp] = tuple.app;
        (*tcp_server)[RtpSession::kStreamID] = tuple.stream;
        (*tcp_server)[RtpSession::kSSRC] = ssrc;
        (*tcp_server)[RtpSession::kOnlyTrack] = only_track;
        if (tcp_mode == PASSIVE) {
            weak_ptr<RtpServer> weak_self = shared_from_this();
            tcp_server->start<RtpSession>(local_port, local_ip, 1024, [weak_self, processor](std::shared_ptr<RtpSession> &session) {
                session->setRtpProcess(processor);
            });
        } else if (tuple.stream.empty()) {
            // tcp主动模式时只能一个端口一个流，必须指定流id; 创建TcpServer对象也仅用于传参  [AUTO-TRANSLATED:61d2a642]
            // In TCP active mode, only one port can have one stream, and the stream ID must be specified; the TcpServer object is created only for parameter passing
            throw std::runtime_error(StrPrinter << "tcp主动模式时必需指定流id");
        }
    }

    _on_cleanup = [rtp_socket]() {
        if (rtp_socket) {
            // 去除循环引用  [AUTO-TRANSLATED:9afaed31]
            // Remove circular references
            rtp_socket->setOnRead(nullptr);
        }
    };

    _tcp_server = tcp_server;
    _udp_server = udp_server;
    _rtp_socket = rtp_socket;
    _rtcp_helper = helper;
    _tcp_mode = tcp_mode;
}

void RtpServer::setOnDetach(RtpProcess::onDetachCB cb) {
    if (_rtcp_helper) {
        _rtcp_helper->setOnDetach(std::move(cb));
    }
}

uint16_t RtpServer::getPort() {
    return _udp_server ? _udp_server->getPort() : _rtp_socket->get_local_port();
}

void RtpServer::connectToServer(const std::string &url, uint16_t port, const function<void(const SockException &ex)> &cb) {
    if (_tcp_mode != ACTIVE || !_rtp_socket) {
        cb(SockException(Err_other, "仅支持tcp主动模式"));
        return;
    }
    weak_ptr<RtpServer> weak_self = shared_from_this();
    _rtp_socket->connect(url, port, [url, port, cb, weak_self](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            cb(SockException(Err_other, "服务对象已释放"));
            return;
        }
        if (err) {
            WarnL << "连接到服务器 " << url << ":" << port << " 失败 " << err;
        } else {
            InfoL << "连接到服务器 " << url << ":" << port << " 成功";
            strong_self->onConnect();
        }
        cb(err);
    },
    5.0F, "::", _rtp_socket->get_local_port());
}

void RtpServer::onConnect() {
    auto rtp_session = std::make_shared<RtpSession>(_rtp_socket);
    rtp_session->setRtpProcess(_rtcp_helper->getProcess());
    rtp_session->attachServer(*_tcp_server);
    _rtp_socket->setOnRead([rtp_session](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) {
        rtp_session->onRecv(buf);
    });
    weak_ptr<RtpServer> weak_self = shared_from_this();
    _rtp_socket->setOnErr([weak_self](const SockException &err) {
        if (auto strong_self = weak_self.lock()) {
            strong_self->_rtp_socket->setOnRead(nullptr);
        }
    });
}

void RtpServer::updateSSRC(uint32_t ssrc) {
    if (_ssrc) {
        *_ssrc = ssrc;
    }

    if (_tcp_server) {
        (*_tcp_server)[RtpSession::kSSRC] = ssrc;
    }
}

uint32_t RtpServer::getSSRC() const {
    if (_ssrc) {
        return *_ssrc;
    }
    if (_tcp_server) {
        return (*_tcp_server)[RtpSession::kSSRC];
    }
    return 0;
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
