#ifndef __RPC_SESSION_H__
#define __RPC_SESSION_H__
#include "socket_stream.h"
#include "mutex.h"
namespace RPC {
class Protocol;
/**
 * @brief 封装服务器端的RPC连接，封装了请求的接收和发送
 * 
 */
class RPCSession : public SocketStream{
public:
    typedef std::shared_ptr<RPCSession> ptr; 
    typedef CoMutex MutexType;
    RPCSession(Socket::ptr socket, bool owner = true);
    std::shared_ptr<Protocol> recvRequest();
    int sendResponse(std::shared_ptr<Protocol> response);
private:
    MutexType mutex_;
};

}




#endif