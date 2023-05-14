#ifndef __RPC_CONNECTION_POOL_H__
#define __RPC_CONNECTION_POOL_H__
#include "rpc/rpc_client.h"
#include "rpc/rpc.h"
#include "rpc/route_strategy.h"
#include "macro.h"
#include "io_manager.h"
namespace RPC{
class RPCConnectionPool : public std::enable_shared_from_this<RPCConnectionPool> {
public:
    typedef std::shared_ptr<RPCConnectionPool> ptr;
    typedef CoMutex MutexType;
    RPCConnectionPool(uint64_t timeout_ms = -1);
    virtual ~RPCConnectionPool();
    /**
     * @brief 连接服务注册中心
     * 
     * @param address 
     * @return true 
     * @return false 
     */
    bool connect(Address::ptr address);

    void close();

    /**
     * @brief RPC 调用过程
     * 
     * @tparam T 
     * @tparam Params 
     * @param name 
     * @param ps 
     * @return Result<T> 
     */
    template<typename T, typename... Params>
    Result<T> call(const std::string &name, Params... ps) {
        MutexType::Lock lock(connect_mutex_);
        auto service = service_map_.find(name);
        Result<T> result;
        if (service != service_map_.end()) {
            lock.unlock();
            result = service->second->call<T>(name, ps...);
            if (result.getCode() != RPC::RPCState::RPC_CLOSED) {
                return result;
            }
            /*移除失效连接*/
            lock.lock();
            std::vector<std::string>& addrs = address_map_[name];
            std::string remote_address_str = service->second->getSocket()->getRemoteAddress()->toString();
            for (auto it = addrs.begin(); it != addrs.end(); ++it) {
                if (*it == remote_address_str) {
                    addrs.erase(it);
                    break;
                }
            }
            service_map_.erase(name);
        }

        /*不存在已有的连接*/
        std::vector<std::string> &addrs = address_map_[name];
        if (addrs.empty()) {
            /*地址列表为空*/
            if (!registry_ || !registry_->isConnected()) {
                result.setCode(RPC::RPCState::RPC_CLOSED);
                result.setMsg("registry closed");
                return result;
            }
            addrs = discover(name);
            if (addrs.empty()) {
                result.setCode(RPC::RPCState::RPC_NO_METHOD);
                result.setMsg("no method " + name);
                return result;
            }
        }
        RouteStrategy<std::string>::ptr strategy = RouteEngine<std::string>::queryStrategy(Strategy::RANDOM);
        /*发现了服务地址*/
        if (addrs.size()) {
            /*TODO: 负载均衡*/
            /*基于随机的策略*/
            const std::string ip = strategy->select(addrs);
            Address::ptr address = Address::LookupAny(ip);
            if (address) {
                RPCClient::ptr rpc_client = std::make_shared<RPCClient>();
                if (rpc_client->connect(address)) {
                    service_map_.emplace(name, rpc_client);
                    lock.unlock();
                    return rpc_client->call<T>(name, ps...);
                }
            }
        }
        result.setCode(RPC::RPCState::RPC_FAIL);
        result.setMsg("call fail");
        return result;
    }

    /**
     * @brief RPC 异步调用过程
     * 
     * @tparam T 
     * @tparam Params 
     * @param name 
     * @param ps 
     * @return Channel<Result<T>> 
     */
    template<typename T, typename... Params>
    Channel<Result<T>> async_call(const std::string &name, Params&&... ps) {
        Channel<Result<T>> chan(1);
        RPCConnectionPool::ptr self = shared_from_this();
        IOManager::GetThis()->Submit([chan, name, ps..., self, this]{
            chan << call<T>(name, ps...);
            self = nullptr;
        });
        return chan;
    }

    /**
     * @brief 订阅服务注册中心的服务
     * 
     * @tparam Func 
     * @param key 
     * @param fun 
     */
    template<typename Func>
    void subscribe(const std::string &key, Func fun) {
        MutexType::Lock lock(subscribe_handle_mutex_);
        auto it = subscribe_handle_.find(key);
        if (it != subscribe_handle_.end()) {
            RPC_ASSERT2(false, "duplicated subscribe");
            return;
        }
        subscribe_handle_.emplace(key, std::move(fun));
        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr requeset = Protocol::Create(RPC::Protocol::MsgType::RPC_SUBSCRIBE_REQUEST, s.toString(), 0);
        channel_ << requeset;
    }

private:
    /**
     * @brief 连接对象的发送协程，负责从channel中收集调用请求，并发送给服务注册中心
     * 
     */
    void handleSend();
    /**
     * @brief 连接对象的接收协程，负责接收注册中心发送的response响应并根据响应类型进行处理
     * 
     */
    void handleRecv();

    /**
     * @brief 处理注册中心的Publish请求
     * 
     * @param response 报文
     */
    void handlePublish(Protocol::ptr response);

    /**
     * @brief 处理注册中心的服务发现响应
     * 
     * @param response 报文
     */
    void handleServiceDiscoverResponse(Protocol::ptr response);
    /**
     * @brief 发现服务
     * 
     * @param name 
     * @return std::vector<std::string> 
     */
    std::vector<std::string> discover(const std::string &name);
private:
    uint64_t timeout_ms_;
    /*注册中心的连接*/
    RPCSession::ptr registry_;
    /*method_map的mutex*/
    MutexType method_map_mutex_;
    /*服务名到调用者协程的映射*/
    std::map<std::string, Channel<Protocol::ptr>> method_map_;
    /*subscribe_handle 的mutex*/
    MutexType subscribe_handle_mutex_;
    /*订阅key到回调函数的映射*/
    std::map<std::string, std::function<void(Serializer)>> subscribe_handle_;
    /*连接池互斥量*/
    MutexType connect_mutex_;
    /*服务名到服务连接的映射*/
    std::map<std::string, RPCClient::ptr> service_map_;
    /*服务到服务地址的映射*/
    std::map<std::string, std::vector<std::string>> address_map_;
    /*是否关闭连接*/
    bool is_closed_;
    /*是否自动开启心跳包*/
    bool auto_heartbeat_;
    /*心跳是否停止*/
    bool is_heartclose_;
    /*到服务注册中心的消息发送通道*/
    Channel<Protocol::ptr> channel_;
    /*心跳包定时器*/
    Timer::ptr heartbeat_timer_;
};
}

#endif