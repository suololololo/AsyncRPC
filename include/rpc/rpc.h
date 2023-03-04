#ifndef __RPC_H__
#define __RPC_H__
#include "rpc/serializer.h"
#include <stdint.h>
#include <sstream>

namespace RPC {
enum RPCState : uint16_t{
        RPC_SUCCESS = 0,    // 成功
        RPC_FAIL,           // 失败
        RPC_ARGS_NOT_MATCH, // 函数参数不匹配
        RPC_NO_METHOD,      // 没有匹配的函数
        RPC_CLOSED,         // 连接已经关闭
        RPC_TIMEOUT,        // 调用超时
};

// 连接池向注册中心订阅的前缀
const char* const RPC_SERVICE_SUBSCRIBE = "[[rpc service subscribe]]";

template<typename T>
struct return_type {
    using type = T;
};
template<>
struct return_type<void> {
    using type = uint8_t;
};

template<typename T>
using return_type_T = typename return_type<T>::type; 



/**
 * @brief 封装RPC调用结果
 * 
 */
template <typename T = void>
class Result{
public:
    using row_type = T;
    using type = return_type_T<T>;
    static Result<T> Success() {
        Result<T> res;
        res.setCode(RPCState::RPC_SUCCESS);
        res.setMsg("success");
        return res;
    }
    static Result<T> Fail() {
        Result<T> res;
        res.setCode(RPCState::RPC_FAIL);
        res.setMsg("fail");
        return res;
    }

    Result() = default;
    Result(RPCState code, const std::string &msg, type val)
        : code_(code), msg_(msg), val_(val){
    }

    void setCode(RPCState code) { code_ = code;}
    void setMsg(const std::string &msg) { msg_ = msg;}
    void setVal(type val) { val_ = val;}
    RPCState getCode() const { return code_;}
    const std::string &getMsg() const { return msg_;}
    type getVal() const { return val_;}


    std::string toString() {
        std::stringstream ss;
        ss << "[ code=" << code_ << " msg=" << msg_ << " val=" << val_ << " ]";
        return ss.str();
    }

    /**
     * @brief 将Result序列化
     * 
     * @param s 
     * @param res 
     * @return Serializer& 
     */
    friend Serializer& operator <<(Serializer &s, const Result<T> &res) {
        s << res.code_ << res.msg_ ;
        if (res.code_ == 0) {
            s << res.val_;
        }
        return s;
    }

    friend Serializer& operator >>(Serializer &s, Result<T> &res) {
        s >> res.code_ >> res.msg_ >> res.val_;
        return s;
    }


private:
    RPCState code_;
    std::string msg_;
    type val_; // 调用结果
};


}




#endif