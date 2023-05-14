#ifndef __RPC_SERVICE_REGISTRY_H__
#define __RPC_SERVICE_REGISTRY_H__
#include "tcp_server.h"
#include "mutex.h"
#include "rpc/protocol.h"
#include "rpc/rpc_session.h"
#include "rpc/serializer.h"
#include <unordered_map>
#include <stdint.h>
namespace RPC {
/**
 * @brief 服务注册中心
 * 
 */
class RPCServiceRegistry : public TCPServer {
public:
    typedef std::shared_ptr<RPCServiceRegistry> ptr;
    typedef CoMutex MutexType;

    RPCServiceRegistry(IOManager* worker = RPC::IOManager::GetThis(), IOManager *accpetWorker = IOManager::GetThis());
    ~RPCServiceRegistry();
    void setName(const std::string &name) override{
        TCPServer::setName(name);
    }
    template <typename T>
    void publish(const std::string &key, T data) {
        {
            MutexType::Lock lock(mutex_);
            if (subscribe_.empty()) {
                return;
            }
        }

        Serializer s;
        s << key <<data;
        s.reset();
        Protocol::ptr response = Protocol::Create(Protocol::MsgType::RPC_PUBLISH_REQUEST, s.toString());
        MutexType::Lock lock(mutex_);
        auto range = subscribe_.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            RPCSession::ptr session = it->second.lock();
            if (!session || !session->isConnected()) {
                continue;
            }
            session->sendResponse(response);
        }

    }

protected:
    /**
     * @brief 处理客户端的事件请求
     * 
     * @param client 
     */
    void handleClient(Socket::ptr client) override;

    /**
     * @brief 添加定时器或更新定时器任务
     * 
     * @param timer 
     * @param client 
     */
    void update(Timer::ptr &timer, Socket::ptr client);

    /**
     * @brief 处理心跳包
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleHeartBeatPacket(Protocol::ptr request);
    /**
     * @brief 处理服务注册请求
     * 将服务器地址注册到对应的服务名下，断开连接后，删除服务和地址
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleServiceRegister(Protocol::ptr request, Address::ptr providerAddr);

    /**
     * @brief 处理服务发现请求
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleServiceDiscover(Protocol::ptr request);

    /**
     * @brief 处理客户端服务订阅
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleSubscribe(Protocol::ptr request, RPCSession::ptr client);

    /**
     * @brief 处理服务器声明为服务提供方的请求
     * 读取报文中的端口后，获取服务器的地址
     * @param request 
     * @param client 
     * @return Address::ptr 
     */
    Address::ptr handleProvider(Protocol::ptr request, Socket::ptr client);

    /**
     * @brief 取消服务的注册
     * 
     * @param providerAddr 
     */
    void handleUnregisterService(Address::ptr providerAddr);
private:
    uint64_t aliveTime_;
    /* 维护服务名和服务地址的多重映射*/
    std::multimap<std::string, std::string> service_;
    MutexType mutex_;

    /* 维护客户端的订阅表*/
    std::unordered_multimap<std::string, std::weak_ptr<RPCSession>> subscribe_;
    /* 维护服务器地址到 服务表项迭代器的映射*/
    std::map<std::string, std::vector<std::multimap<std::string, std::string>::iterator>> iters_;
};


}



#endif