#ifndef __RPC_CLIENT_H__
#define __RPC_CLIENT_H__
#include "rpc/rpc.h"
#include "rpc/serializer.h"
#include "rpc/rpc_session.h"
#include "rpc/protocol.h"
#include "channel.h"
#include "assert.h"
#include "timer.h"

#include <memory>
#include <functional>
namespace RPC {


class RPCClient : public std::enable_shared_from_this<RPCClient>{
public:
    typedef CoMutex MutexType;
    typedef std::shared_ptr<RPCClient> ptr;

    RPCClient(bool auto_heartbeat = true);
    virtual ~RPCClient();

    /**
     * @brief 连接RPC服务器，并启动协程对消息发送通道的发送和接收
     * 
     * @return true 
     * @return false 
     */
    bool connect(Address::ptr address);
    
    void close();

    void setTimeout(uint64_t timeout_ms);

    /**
     * @brief 带参数调用
     * 
     * @tparam T 
     * @tparam Params 
     * @param name 
     * @param ps 
     * @return Result<T> 
     */
    template <typename T, typename... Params>
    Result<T> call(const std::string &name, Params... ps) {
        using args_type = std::tuple<typename std::decay<Params>::type...>;
        args_type args = std::make_tuple(ps...);
        Serializer s;
        s << name << args;
        s.reset();
        return call<T>(s);
    }

    /**
     * @brief 无参数调用
     * 
     * @tparam T 
     * @param name 
     * @return Result<T> 
     */
    template <typename T>
    Result<T> call(const std::string &name) {
        Serializer s;
        s << name;
        s.reset();
        return call<T>(s);
    }


    template <typename Func>
    void subscribe(const std::string &key, Func func) {
        {
            MutexType::Lock lock(mutex_);
            auto it = subscribe_map_.find(key);
            if (it != subscribe_map_.end()) {
                // RPC_ASSERT2(false, "duplicate subscribe");
                // RPC_LOG_ERROR(logger) << "duplicate subscribe";
                return;
            }

            subscribe_map_.emplace(key, std::move(func));
        }

        Serializer s;
        s << key;
        s.reset();
        Protocol::ptr requeset = Protocol::Create(RPC::Protocol::MsgType::RPC_SUBSCRIBE_REQUEST, s.toString());
        channel_ << requeset;
    }

    Socket::ptr getSocket() {
        return session_->getSocket();
    }

private:
    template <typename T>
    Result<T> call(Serializer &s) {
        Result<T> val;
        if (!session_ || !session_->isConnected()) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        Channel<Protocol::ptr> recv_channel(1);
        std::map<uint32_t, Channel<Protocol::ptr>>::iterator it;
        /* 请求序列号*/
        uint32_t id;
        {
            MutexType::Lock lock(mutex_);
            id = sequence_id_;
            it = response_handle_.emplace(id, recv_channel).first;
            sequence_id_++;
        }

        Protocol::ptr request = Protocol::Create(Protocol::MsgType::RPC_METHOD_REQUEST, s.toString(), id);
        channel_ << request;

        Protocol::ptr response;
        bool timeout = false;
        if (!recv_channel.waitFor(timeout_ms_, response)) {
            timeout = true;
        }
        {
            MutexType::Lock lock(mutex_);
            if (!is_closed_) {
                response_handle_.erase(it);
            }
        }
        if (timeout) {
            val.setCode(RPC_TIMEOUT);
            val.setMsg("call timeout");
            return val;
        }

        if (!response) {
            val.setCode(RPC_CLOSED);
            val.setMsg("socket closed");
            return val;
        }

        if (response->getContent().empty()) {
            val.setCode(RPC_NO_METHOD);
            val.setMsg("method not find");
            return val;
        }
        Serializer seria(response->getContent());

        try {
            seria >> val;
        } catch(...) {
            val.setCode(RPC_ARGS_NOT_MATCH);
            val.setMsg("return value not match");
        }
        return val;
    }


    void handleSend();

    void handleRecv();

    void handleMethodResponse(Protocol::ptr response);

    // void handleServiceDiscoverResponse(Protocol::ptr response);

    void handlePublish(Protocol::ptr response);
private:
    /* 与服务器的连接*/
    RPCSession::ptr session_;
    CoMutex mutex_;
    uint32_t sequence_id_;
    /* 请求序列号和调用者协程channel的映射*/
    std::map<uint32_t, Channel<Protocol::ptr>> response_handle_;
    /* 消息发送通道*/
    Channel<Protocol::ptr> channel_;

    /*超时时间*/
    uint64_t timeout_ms_;
    /*是否关闭连接*/
    bool is_closed_;
    /*是否自动开启心跳包*/
    bool auto_heartbeat_;
    /*心跳是否停止*/
    bool is_heartclose_;



    /*订阅表*/
    std::map<std::string, std::function<void(Serializer)>> subscribe_map_;

    /*心跳定时器*/
    Timer::ptr heartbeat_timer_;
};


}


#endif