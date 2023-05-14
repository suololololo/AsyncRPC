#include "rpc/rpc_server.h"
#include "rpc/rpc.h"
#include "log.h"
#include "hook.h"
#include <iostream>

namespace RPC {
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
static uint64_t s_heartbeat_timeout = 40000;

RPCServer::RPCServer(IOManager* worker, IOManager *acceptWorker):TCPServer(worker, acceptWorker), alive_time_(s_heartbeat_timeout), stop_clean_(false), clean_channel_(1) {

}
RPCServer::~RPCServer() {
    {
        MutexType::Lock lock(mutex_);
        stop_clean_ = true;
    }
    bool join = false;
    clean_channel_ >> join;
}

bool RPCServer::bind(Address::ptr address) {
    port_ = std::dynamic_pointer_cast<IPv4Address>(address)->getPort();
    return TCPServer::bind(address);
}
bool RPCServer::start() {
    if (registry_) {
        for (auto &it :services_) {
            RPC_LOG_DEBUG(logger) << "register service: " << it.first;
            registerService(it.first);
        }

        registry_->getSocket()->setRecvTimeout(30'000);
        auto server = std::dynamic_pointer_cast<RPCServer>(shared_from_this());
        // 每30秒发一个心跳包
        heartTimer_ = worker_->addTimer(30'000, [server] {
            RPC_LOG_DEBUG(logger) << "heart beat";
            Protocol::ptr heartbeat = Protocol::HeartBeat();
            server->registry_->sendResponse(heartbeat);
            Protocol::ptr response = server->registry_->recvRequest();
            // 没有回复认为注册中心挂了
            if (!response) {
                RPC_LOG_DEBUG(logger) << "registry close";
                server->heartTimer_->cancel();
            }
        }, true);
    }

    /*开启协程定时清理订阅列表*/
    RPC::IOManager::GetThis()->Submit([this]() {
        while (!stop_clean_) {
            sleep(5);
            MutexType::Lock lock(mutex_);
            for (auto it = subscribes_.cbegin(); it != subscribes_.cend();) {
                auto conn = it->second.lock();
                if (!conn || !conn->isConnected()) {
                    it = subscribes_.erase(it);
                } else {
                    it++;
                }
            }
        }
        clean_channel_ << true;
    });
    return TCPServer::start();
}

void RPCServer::update(Timer::ptr &heartTimer, Socket::ptr client) {
    if (!heartTimer) {
        heartTimer = worker_->addTimer(alive_time_, [client]{
            RPC_LOG_DEBUG(logger) << "client: "<< *client << " closed";
            client->close();
        });
        return;
    }
    heartTimer->reset(alive_time_, true);
}

void RPCServer::handleClient(Socket::ptr client) {
    RPC_LOG_INFO(logger) << "handle client :" << *client;
    RPCSession::ptr session = std::make_shared<RPCSession>(client);
    Timer::ptr heartTimer;
    update(heartTimer, client);
    while(true) {
        Protocol::ptr request = session->recvRequest();
        if (request == nullptr) {
            break;
        }
        Protocol::ptr response;
        update(heartTimer, client);
        // HEARTBEAT_PACKET,  //心跳包
        // RPC_REQUEST,       //RPC 通用请求包
        switch (request->getMsgType()) {
            // 心跳包请求
            case Protocol::MsgType::HEARTBEAT_PACKET:
            {
                response = handleHeartBeatPacket(request);
                break;
            }

            // 方法调用请求
            case Protocol::MsgType::RPC_METHOD_REQUEST:
            {
                response = handleMethodCall(request);
                break;
            }

            // 订阅请求
            case Protocol::MsgType::RPC_SUBSCRIBE_REQUEST:
            {
                response = handleSubscribe(request, session);
                break;
            }
            break;
            // 发布响应
            case Protocol::MsgType::RPC_PUBLISH_RESPONSE:
                return;
            default:
                RPC_LOG_INFO(logger) << "protocol = " << request->toString();
            break;
        }
        if (response) {
            session->sendResponse(response);
        }

    }
}

Protocol::ptr RPCServer::handleHeartBeatPacket(Protocol::ptr request) {
    return Protocol::HeartBeat();
}

Protocol::ptr RPCServer::handleMethodCall(Protocol::ptr request) {
    /* 序列化数据格式： 函数名+函数参数*/
    Serializer s(request->getContent());
    std::string funName;
    s >>funName;
    if (services_.find(funName) != services_.end()) {

    }
    Serializer rt = call(funName, s.toString());
    Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_METHOD_RESPONSE, rt.toString(), request->getSequenceId());

    return response;

}

Protocol::ptr RPCServer::handleSubscribe(Protocol::ptr request, RPCSession::ptr client) {
    Protocol::ptr response;
    MutexType::Lock lock(mutex_);
    Serializer s(request->getContent());
    std::string key;
    s >> key;
    subscribes_.emplace(key, std::weak_ptr<RPCSession>(client));
    Result<> res = Result<>::Success();
    s.reset();
    s << res;
    return Protocol::Create(Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE, s.toString(), request->getSequenceId());
    
}



bool RPCServer::connectRegistry(Address::ptr addr) {
    Socket::ptr sock = Socket::CreateTCP(addr);

    if (!sock) {
        return false;
    }
    if (!sock->connect(addr)) {
        RPC_LOG_ERROR(logger) << "can not connect to registry, errno=" << errno
                            << " strerr=" << strerror(errno); 
        registry_ = nullptr;
        return false;
    }

    registry_ = std::make_shared<RPCSession>(sock);
    RPC_LOG_DEBUG(logger) << "connect registry success, registry address" << registry_->getSocket();
    Serializer s;
    s << port_;
    s.reset();
    Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_PROVIDER, s.toString());
    registry_->sendResponse(response);
    return true;
}

void RPCServer::registerService(const std::string &name) {
    Protocol::ptr request = Protocol::Create(Protocol::MsgType::RPC_SERVICE_REGISTER, name);
    registry_->sendResponse(request);
    Protocol::ptr response = registry_->recvRequest();

    if (!response) {
        RPC_LOG_DEBUG(logger) << "register Service response error, errno=" << errno << " strerr=" << strerror(errno);
        return;
    }
    
    Result<std::string> result;
    Serializer s(response->getContent());
    s >> result;

    if (result.getCode() != RPC_SUCCESS) {
        RPC_LOG_WARN(logger) << result.toString();
    }
    RPC_LOG_DEBUG(logger) << result.toString();
    
}

void RPCServer::setName(const std::string &name) {

}

Serializer RPCServer::call(const std::string &funName, const std::string &args) {
    Serializer res;
    if (services_.find(funName) == services_.end()) {
        return res;
    }
    auto fun = services_[funName];
    fun(res, args);
    res.reset();
    return res;
}


}