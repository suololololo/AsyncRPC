#include "rpc/rpc_connection_pool.h"
#include "io_manager.h"
#include "log.h"
// #include <vector>
namespace RPC{
static uint64_t s_channel_capacity = 1;
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
RPCConnectionPool::RPCConnectionPool(uint64_t timeout_ms):timeout_ms_(timeout_ms), auto_heartbeat_(true), channel_(s_channel_capacity) {

}

RPCConnectionPool::~RPCConnectionPool() {
    close();
}

bool RPCConnectionPool::connect(Address::ptr address) {
    Socket::ptr sock = Socket::CreateTCP(address);
    if (!sock) {
        return false;
    }
    if (!sock->connect(address, timeout_ms_)) {
        sock = nullptr;
        return false;
    }
    registry_ = std::make_shared<RPCSession>(sock);
    channel_ = Channel<Protocol::ptr>(s_channel_capacity);
    is_closed_ = false;
    is_heartclose_ = false;
    RPC::IOManager::GetThis()->Submit([this]{
        handleSend();
    });
    RPC::IOManager::GetThis()->Submit([this]{
        handleRecv();
    });

    if (auto_heartbeat_) {
        heartbeat_timer_ = RPC::IOManager::GetThis()->addTimer(30'000, [this]() {
            RPC_LOG_DEBUG(logger) << "heart beat";
            if (is_heartclose_) {
                RPC_LOG_INFO(logger) << "registry closed";
                close();
            }
            Protocol::ptr hearbeat_packet = Protocol::HeartBeat();
            channel_ << hearbeat_packet;
        }, true);
    }
    return true;
}

void RPCConnectionPool::close() {
    RPC_LOG_DEBUG(logger) << "rpc connection close";
    MutexType::Lock lock(connect_mutex_);
    if (is_closed_) {
        return;
    }
    is_closed_ = true;
    is_heartclose_ = true;
    channel_.close();
    if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
        heartbeat_timer_ = nullptr;
    }
    for (auto it : service_map_) {
        it.second->close();
    }
    RPC::IOManager::GetThis()->delEvent(registry_->getSocket()->getSocket(), RPC::IOManager::Event::READ);
    registry_->close();
}



void RPCConnectionPool::handleSend() {
    Protocol::ptr requeset;
    while (channel_ >> requeset) {
        if (!requeset) {
            RPC_LOG_WARN(logger) << "RPCClient::handleSend() handle send fail";
            continue;
        }
        registry_->sendResponse(requeset);
    }
}
void RPCConnectionPool::handleRecv() {
    if (!registry_->isConnected()) {
        RPC_LOG_WARN(logger) << "RPCClient::handleRecv() handle recv fail";
        return;
    }

    while (true) {
        Protocol::ptr request = registry_->recvRequest();
        if (!request) {
            RPC_LOG_DEBUG(logger) << "RPCClient::handleRecv() recv request fail";
            close();
            return;
        }
        RPC::Protocol::MsgType type = request->getMsgType();
        is_heartclose_ = false;
        switch(type) {
            case RPC::Protocol::MsgType::HEARTBEAT_PACKET:
                is_heartclose_ = false;
                break;
            case RPC::Protocol::MsgType::RPC_PUBLISH_REQUEST:
                handlePublish(request);
                channel_ << Protocol::Create(RPC::Protocol::MsgType::RPC_PUBLISH_RESPONSE, "");
                break;
            case RPC::Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE:
                handleServiceDiscoverResponse(request);
                break;
            case RPC::Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE:
                break;
            default:
                RPC_LOG_DEBUG(logger) << "protocol" << request->toString();
                break;
        }
    }
    
}



/**
 * @brief 处理注册中心的Publish请求
 * 
 */
void RPCConnectionPool::handlePublish(Protocol::ptr response) {
    Serializer s(response->getContent());
    std::string key;
    s >> key;
    std::map<std::string, std::function<void(Serializer)>>::iterator it;
    MutexType::Lock lock(subscribe_handle_mutex_);   
    it = subscribe_handle_.find(key);
    if (it == subscribe_handle_.end()) {
        return;
    }
    it->second(s);
}

    /**
     * @brief 处理注册中心的服务发现响应
     * 
     */
void RPCConnectionPool::handleServiceDiscoverResponse(Protocol::ptr response) {
    Serializer s(response->getContent());
    std::string service_name;
    // int cnt = 0; // 发现的服务数量
    s >> service_name;
    // s >> cnt;
    std::map<std::string, Channel<Protocol::ptr>>::iterator it;
    MutexType::Lock lock(method_map_mutex_);
    it = method_map_.find(service_name);
    if (it == method_map_.end()) {
        return;
    }
    // 获取等待该结果的channel通道，将报文传入channel,让在该channel上挂起的协程自行处理
    Channel<Protocol::ptr> chan = it->second;
    chan << response;

}

    /**
     * @brief 发现服务
     * 
     * @param name 
     * @return std::vector<std::string> 
     */
std::vector<std::string> RPCConnectionPool::discover(const std::string &name) {
    if (!registry_ || !registry_->isConnected()) return {};
    // 创建接收信息的通道
    Channel<Protocol::ptr> recv_chan(1);
    std::map<std::string, Channel<Protocol::ptr>>::iterator it;
    {
        MutexType::Lock lock(method_map_mutex_);
        it = method_map_.emplace(name, recv_chan).first;
    }
    // 发送请求报文
    Protocol::ptr request = Protocol::Create(RPC::Protocol::MsgType::RPC_SERVICE_DISCOVER, name);
    channel_ << request;

    // 接收响应报文
    Protocol::ptr response;
    recv_chan >> response;
    
    {
        MutexType::Lock lock(method_map_mutex_);
        method_map_.erase(it);
    }
    if (!response) return {};

    std::vector<std::string> result;
    Serializer s(response->getContent());
    std::string service_name;
    uint32_t cnt = 0;
    s >> service_name >> cnt;
    bool flag = true;
    for (uint32_t i = 0; i < cnt; ++i) {
        Result<std::string> res;
        s >> res;
        if (res.getCode() == RPC_NO_METHOD) {
            flag = false;
            break;
        }
        result.push_back(res.getVal());
    }

    if (!flag) return {};

    // 向服务中心订阅服务变化的信息
    if (subscribe_handle_.count(RPC_SERVICE_SUBSCRIBE + name) == 0) {
        subscribe(RPC_SERVICE_SUBSCRIBE + name, [name, this](Serializer s) {
            // 服务是否下线
            bool service_launch = true;
            std::string addr;
            s >> service_launch >> addr;
            MutexType::Lock lock(connect_mutex_);
            if (!service_launch) {
                // 服务下线
                RPC_LOG_DEBUG(logger) << "service [ " << name << " : " << addr << " ] quit";
                address_map_[name].push_back(addr);
            } else {
                // 服务上线
                RPC_LOG_DEBUG(logger) << "service [ " << name << " : " << addr << " ] join";
                auto it = address_map_.find(name);
                if (it != address_map_.end()) {
                    for (auto i = it->second.begin(); i != it->second.end(); ++i) {
                        if (*i == addr) {
                            it->second.erase(i);
                            break;
                        }
                    }
                }
            }
        });
    }
    return result;

}

}