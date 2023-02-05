#ifndef __SOCKET_STREAM_H__
#define __SOCKET_STREAM_H__
#include "socket.h"
#include "stream.h"
namespace RPC {
class SocketStream : public Stream {
public:
    typedef std::shared_ptr<SocketStream> ptr;
    SocketStream(Socket::ptr socket, bool owner);
    ~SocketStream();
    virtual int read(void *buff, size_t len) override;
    virtual int read(ByteArray::ptr ba, size_t len) override;
    virtual int write(const void *buff, size_t len) override;
    virtual int write(ByteArray::ptr ba, size_t len) override;
    virtual void close() override;
    bool isConnected() const { return socket_ && socket_->isConnected();}
    Socket::ptr getSocket() { return socket_;}
private:
    Socket::ptr socket_;
    bool owner_; // socketstream析构所有权
};
}


#endif