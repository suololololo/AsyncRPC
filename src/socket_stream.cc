#include "socket_stream.h"

namespace RPC {
SocketStream::SocketStream(Socket::ptr socket, bool owner):socket_(socket), owner_(owner) {

}
SocketStream::~SocketStream() {
    if (socket_ && owner_) {
        socket_->close();
    }
}    
int SocketStream::read(void *buff, size_t len) {
    if (!isConnected()) {
        return -1;
    }
    return socket_->recv(buff, len);
}
int SocketStream::read(ByteArray::ptr ba, size_t len) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iov;
    ba->getWriteBuffers(iov, len);
    int ret = socket_->recv(&iov[0], iov.size());
    if (ret > 0) {
        ba->setPosition(ba->getPosition() + ret);
    }
    return ret;
}
int SocketStream::write(const void *buff, size_t len) {
    if (!isConnected()) {
        return -1;
    }
    return socket_->send(buff, len);
}
int SocketStream::write(ByteArray::ptr ba, size_t len) {
    if (!isConnected()) {
        return -1;
    }
    std::vector<iovec> iov;
    ba->getReadBuffers(iov, len);
    int ret = socket_->send(&iov[0], iov.size());
    if (ret > 0) {
        ba->setPosition(ba->getPosition() + ret);
    }
    return ret;
}    

void SocketStream::close() {
    if (socket_) {
        socket_->close();
    }
}

}