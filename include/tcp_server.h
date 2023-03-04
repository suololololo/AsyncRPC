#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include "socket.h"
#include "address.h"
#include "io_manager.h"
#include "noncopyable.h"
// #include <memory>

namespace RPC {
class TCPServer : public std::enable_shared_from_this<TCPServer>, Noncopyable{
public:
    typedef std::shared_ptr<TCPServer> ptr;
    TCPServer(IOManager* worker = RPC::IOManager::GetThis(), IOManager *accpetWorker = IOManager::GetThis());
    virtual ~TCPServer();
    virtual bool bind(RPC::Address::ptr addr);
    virtual bool bind(const std::vector<RPC::Address::ptr> &addrs, std::vector<RPC::Address::ptr> &fail);
    virtual bool start();
    virtual bool stop();
    virtual void startAccept(Socket::ptr sock);
    virtual void setName(const std::string &name) { name_ = name;}
    std::string getName() { return name_;}
protected:
    virtual void handleClient(Socket::ptr client);
protected:
    std::vector<Socket::ptr> socks_;
    IOManager* worker_;
    IOManager* acceptWorker_;
    bool stop_;
    std::string name_; // 服务器的名称
};


}


#endif