#include "rpc/rpc_service_registry.h"
#include "rpc/rpc.h"
#include "log.h"

namespace RPC {
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
static uint64_t s_heartbeat_timeout = 40000;


RPCServiceRegistry::RPCServiceRegistry(IOManager* worker, IOManager *accpetWorker):TCPServer(worker, accpetWorker), aliveTime_(s_heartbeat_timeout) {

}

RPCServiceRegistry::~RPCServiceRegistry() {

}

void RPCServiceRegistry::handleClient(Socket::ptr client) {
    RPC_LOG_DEBUG(logger) << "handler client, client address: " << client;
    RPCSession::ptr session = std::make_shared<RPCSession> (client);
    Address::ptr providerAddr;
    Timer::ptr heartbeat_timer;
    /* 开启定时器任务*/
    update(heartbeat_timer, client);
    while (true) {
        Protocol::ptr request = session->recvRequest();
        if (!request) {
            if (providerAddr) {
                RPC_LOG_DEBUG(logger) << client << " was closed; unregister " << providerAddr->toString();
                handleUnregisterService(providerAddr);
            }
            return;
        }
        update(heartbeat_timer, client);
        Protocol::MsgType type = request->getMsgType();
        Protocol::ptr response;
        switch (type) {
            // 心跳包
            case Protocol::MsgType::HEARTBEAT_PACKET: 
                response = handleHeartBeatPacket(request);
                break;
            
            case Protocol::MsgType::RPC_PROVIDER:
                providerAddr = handleProvider(request, client);
                continue;
            
            // 服务器 服务注册请求
            case Protocol::MsgType::RPC_SERVICE_REGISTER: 
                response = handleServiceRegister(request, providerAddr);
                break;
            
            // 客户端 服务发现请求
            case Protocol::MsgType::RPC_SERVICE_DISCOVER: 
                response = handleServiceDiscover(request);
                break;
            
            // 客户端 服务订阅
            case Protocol::MsgType::RPC_SUBSCRIBE_REQUEST: 
                response = handleSubscribe(request, session);
                break;
            
            // 向客户端推送服务的响应
            case Protocol::MsgType::RPC_PUBLISH_RESPONSE: 
                continue;
            
            default: 
                RPC_LOG_WARN(logger) << "protocol msg error, protocol: " << request->toString(); 
                continue;
        }
        session->sendResponse(response);
    }
}


void RPCServiceRegistry::update(Timer::ptr &timer, Socket::ptr client) {
    if (!timer) {
        timer = worker_->addTimer(aliveTime_, [client] {
            RPC_LOG_INFO(logger) << "client: " << client << " closed";
            client->close();
        });
        return;
    }
    // 更新定时器时间
    timer->reset(aliveTime_, true);
}

Protocol::ptr RPCServiceRegistry::handleHeartBeatPacket(Protocol::ptr request) {
    return Protocol::HeartBeat();
}

Address::ptr RPCServiceRegistry::handleProvider(Protocol::ptr request, Socket::ptr client) {
    Serializer s(request->getContent());
    s.reset();
    uint32_t port = 0;
    s >> port;
    IPv4Address::ptr address(new IPv4Address(*std::dynamic_pointer_cast<IPv4Address>(client->getRemoteAddress())));
    address->setPort(port);
    return address;
}

Protocol::ptr RPCServiceRegistry::handleServiceRegister(Protocol::ptr request, Address::ptr providerAddr) {
    std::string service_name = request->getContent();
    std::string service_address = providerAddr->toString();
    RPC_LOG_DEBUG(logger) << "register service, server: " <<providerAddr << " service name: " << service_name;
    {
        MutexType::Lock lock(mutex_);
        auto it = service_.emplace(service_name, service_address);
        iters_[service_address].push_back(it) ;
    }



    Result<std::string> res = Result<std::string>::Success();

    res.setVal(service_name);

    Serializer s;
    s << res;
    s.reset();
    Protocol::ptr proto = Protocol::Create(RPC::Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE, s.toString());

    // 发布服务上线消息
    std::tuple<bool, std::string> data { true, service_address};
    publish(RPC_SERVICE_SUBSCRIBE + service_name, data);

    return proto;
}

Protocol::ptr RPCServiceRegistry::handleServiceDiscover(Protocol::ptr request) {
    std::string service_name = request->getContent();
    std::vector<Result<std::string>> result;
    MutexType::Lock lock(mutex_);
    auto range = service_.equal_range(service_name);
    uint32_t cnt = 0;

    if (range.first == range.second) {
        Result<std::string> res;
        res.setCode(RPC_NO_METHOD);
        res.setMsg("discover service: " +service_name);
        result.push_back(res);
        cnt++;
    } else {
        for (auto i = range.first; i != range.second; ++i) {
            Result<std::string> res;
            res.setCode(RPC_SUCCESS);
            res.setVal(i->second);
            result.push_back(res);
        }
        cnt = result.size();
    }

    Serializer s;
    s << service_name << cnt;
    for (uint32_t i = 0; i < cnt; ++i) {
        s << result[i];
    }
    s.reset();
    Protocol::ptr proto = Protocol::Create(Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE, s.toString());
    return proto;

}
/**
 * @brief 处理客户端服务订阅
 * 
 * @param request 
 * @return Protocol::ptr 
 */
Protocol::ptr RPCServiceRegistry::handleSubscribe(Protocol::ptr request, RPCSession::ptr client) {
    // 获取订阅的key
    std::string key;
    MutexType::Lock lock(mutex_);
    Serializer s(request->getContent());
    s>> key;
    subscribe_.emplace(key, std::weak_ptr<RPCSession>(client));
    Result<> res = Result<>::Success();
    s.reset();
    s << res;
    Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE, s.toString());
    return response;

}

void RPCServiceRegistry::handleUnregisterService(Address::ptr providerAddr) {
    MutexType::Lock lock(mutex_);
    auto it = iters_.find(providerAddr->toString());
    if (it == iters_.end()) {
        return;
    } 

    auto its = it->second;
    for (auto &i :its) {
        service_.erase(i);
        // 服务下线通知
        std::tuple<bool, std::string> data{false, providerAddr->toString()};
        publish(RPC_SERVICE_SUBSCRIBE+i->first, data);
    }
    iters_.erase(providerAddr->toString());
}

}