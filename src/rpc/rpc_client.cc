#include "rpc/rpc_client.h"
#include "log.h"
#include "io_manager.h"
namespace RPC {

static RPC::Logger::ptr logger = RPC_LOG_ROOT();
static uint64_t s_channel_capacity = 2;


RPCClient::RPCClient(bool auto_heartbeat):sequence_id_(0), channel_(s_channel_capacity), timeout_ms_(-1), auto_heartbeat_(auto_heartbeat){

}

RPCClient::~RPCClient() {
    close();
}

    /**
     * @brief 连接RPC服务器，并启动协程对消息发送通道的发送和接收
     * 
     * @return true 
     * @return false 
     */
bool RPCClient::connect(Address::ptr address) {
    Socket::ptr sock = Socket::CreateTCP(address);

    if (!sock) {
        return false;
    }

    if (!sock->connect(address, timeout_ms_)) {
        session_ = nullptr;
        return false;
    }

    session_ = std::make_shared<RPCSession>(sock);
    RPC_LOG_DEBUG(logger) << "server address" << *session_->getSocket();
    channel_ = Channel<Protocol::ptr>(s_channel_capacity);
    is_heartclose_ = false;
    is_closed_ = false;
    /*处理通道消息的接收和发送*/
    RPC::IOManager::GetThis()->Submit([this]{
        handleSend();
    });
    RPC::IOManager::GetThis()->Submit([this]{
        handleRecv();
    });

    if (auto_heartbeat_) {
        heartbeat_timer_ = IOManager::GetThis()->addTimer(30'000, [this]() {
            RPC_LOG_DEBUG(logger) << "heart beat";
            if (is_heartclose_) {
                RPC_LOG_INFO(logger) << "server closed";
                close();
            }
            Protocol::ptr heart_beat = Protocol::HeartBeat();
            channel_ << heart_beat;
            is_heartclose_ = true;
        }, true);
    }

    return true;
}
    
void RPCClient::close() {
    RPC_LOG_DEBUG(logger) << "client close";
    MutexType::Lock lock(mutex_);
    if (is_closed_) {
        return;
    }
    is_heartclose_ = true;
    is_closed_ = true;
    channel_.close();
    for (auto it : response_handle_) {
        it.second << nullptr;
    }
    response_handle_.clear();

    if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
        heartbeat_timer_ = nullptr;
    }
    IOManager::GetThis()->delEvent(session_->getSocket()->getSocket(), IOManager::Event::READ);
    session_->close();
}

void RPCClient::setTimeout(uint64_t timeout_ms) {
    MutexType::Lock lock(mutex_);
    timeout_ms_ = timeout_ms;
}

void RPCClient::handleSend() {
    Protocol::ptr request;
    while (channel_ >> request) {
        if (!request) {
            RPC_LOG_WARN(logger) << "RPCClient::handleSend() handle send fail";
            continue;
        }
        RPC_LOG_DEBUG(logger) << "handle send";
        session_->sendResponse(request);
    }
}

void RPCClient::handleRecv() {
    if (!session_->isConnected()) {
        RPC_LOG_WARN(logger) << "RPCClient::handleRecv() handle recv fail";
        return;
    }

    while (true) {
        Protocol::ptr response = session_->recvRequest();
        if (!response) {
            RPC_LOG_WARN(logger) << "RPCClient::handleRecv() recv request fail";
            close();
            continue;;
        }
        RPC::Protocol::MsgType type = response->getMsgType();
        // RPC_LOG_DEBUG(logger) << "msg type: " << response->toString();
        is_heartclose_ = false;
        switch(type) {
            /*订阅响应*/
            case Protocol::MsgType::RPC_SUBSCRIBE_RESPONSE:
                break;
            case Protocol::MsgType::HEARTBEAT_PACKET:
                is_heartclose_ = false;
                break;
            case Protocol::MsgType::RPC_METHOD_RESPONSE:
                handleMethodResponse(response);
                break;
            case Protocol::MsgType::RPC_PUBLISH_REQUEST:
                handlePublish(response);
                channel_ << Protocol::Create(RPC::Protocol::MsgType::RPC_PUBLISH_RESPONSE, "");
                break;
            // case Protocol::MsgType::RPC_SERVICE_DISCOVER_RESPONSE:
            //     handleServiceDiscoverResponse(response);
            //     break;
            default:
                RPC_LOG_DEBUG(logger) << "protocol" << response->toString();
                break;
        }

    }

}


void RPCClient::handleMethodResponse(Protocol::ptr response) {
    sequence_id_ = response->getSequenceId();
    
    std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;
    MutexType::Lock lock(mutex_);
    it = response_handle_.find(sequence_id_);
    if (it == response_handle_.end()) {
        return;
    }
    Channel<Protocol::ptr> channel = it->second;
    channel << response;
}


void handleServiceDiscoverResponse(Protocol::ptr response) {

}

void RPCClient::handlePublish(Protocol::ptr response) {
    Serializer s(response->getContent());
    std::string key;
    s >> key;
    std::map<std::string, std::function<void(Serializer)>>::iterator it;
    MutexType::Lock lock(mutex_);
    it = subscribe_map_.find(key);
    if (it == subscribe_map_.end()) {
        return;
    }
    it->second(s);
}

}
