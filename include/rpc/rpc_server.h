#ifndef __RPC_SERVER_H__
#define __RPC_SERVER_H__
#include "tcp_server.h"
#include "traits.h"
#include "rpc/rpc.h"
#include "rpc/rpc_session.h"
#include "rpc/protocol.h"
#include "rpc/serializer.h"
#include "mutex.h"
namespace RPC {
/**
 * @brief 提供服务的RPC服务端
 * 
 */
class RPCServer : public TCPServer {
public:
    typedef std::shared_ptr<RPCServer> ptr;
    typedef CoMutex MutexType; 
    RPCServer(IOManager* worker = IOManager::GetThis(), IOManager *acceptWorker = IOManager::GetThis());
    ~RPCServer() {};
    /**
     * @brief 函数注册
     * 
     * @param funName 函数名
     * @param fun 
     */
    template<typename Fun>
    bool registerMethod(const std::string &funName, Fun fun) {
        services_[funName] = [fun, this](Serializer serializer, const std::string &arg) {
            proxy(fun, serializer, arg);
        };
        return true;
    }
    void setName(const std::string &name) override;

    /**
     * @brief 跟服务注册中心连接
     * 
     */
    bool connectRegistry(Address::ptr addr);
    bool start() override;
    bool bind(Address::ptr address) override;
    
protected:
    virtual void handleClient(Socket::ptr client) override;
    /**
     * @brief 向服务中心注册注册服务
     * 
     */
    void registerService(const std::string &name);


    /**
     * @brief 处理客户端心跳包
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleHeartBeatPacket(Protocol::ptr request);
    /**
     * @brief 处理客户端方法调用
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleMethodCall(Protocol::ptr request);
    /**
     * @brief 处理客户端订阅请求
     * 
     * @param request 
     * @return Protocol::ptr 
     */
    Protocol::ptr handleSubscribe(Protocol::ptr request, RPCSession::ptr client);
    /**
     * @brief 根据函数名和函数参数调用RPC服务
     * 
     * @param funName 
     * @param args 
     * @return Serializer 
     */
    Serializer call(const std::string &funName, const std::string &args);

    /* RPC过程实际调用服务端提供函数的过程 */
    template<typename Fun>
    void proxy(Fun func, Serializer serializer, const std::string &arg) {
        // 反序列化函数参数
        RPC::Serializer s(arg);
        using return_type = typename function_trait<Fun>::return_type;
        using Args = typename function_trait<Fun>::arg_tuple_type;
        typename function_trait<Fun>::stl_function_type fun = func;

        Args args;
        try {
            s >> args;
        } catch (...) {
            Result<return_type> res;
            res.setCode(RPCState::RPC_ARGS_NOT_MATCH);
            res.setMsg("args not match");
            serializer << res;
            return ;
        }

        /* 调用函数 */
        return_type_T<return_type> rt{};
        /* 元组参数数量 */
        constexpr auto size = std::tuple_size<typename std::decay<Args>::type>::value;

        // template<typename Tuple, size_t N>
        // struct 


        auto invoke = [&fun, &args]<std::size_t... Index>(std::index_sequence<Index...>) {
            return fun(std::get<Index>(std::forward<Args>(args))...);
        };
        if (std::is_same<void, return_type>::value) {
            invoke(std::make_index_sequence<size>{});
        } else {
            rt = invoke(std::make_index_sequence<size>{});
        }

        Result<return_type> res;
        res.setCode(RPCState::RPC_SUCCESS);
        res.setVal(rt);
        serializer << res;
    }


private:
    /* 注册函数表 */
    std::map<std::string, std::function<void(Serializer, std::string)>> services_; 
    /* 服务注册中心 */
    RPCSession::ptr registry_;
    /* 服务提供端口 */
    uint32_t port_;
    /* 订阅的客户端 */
    std::unordered_map<std::string, std::weak_ptr<RPCSession>> subscribes_;

    MutexType mutex_;
    /* 心跳包间隔时间 */
    uint64_t alive_time_;
    /* 心跳包定时器*/
    Timer::ptr heartTimer_;

};

}

#endif