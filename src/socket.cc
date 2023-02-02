#include "socket.h"
#include "log.h"
#include "hook.h"
#include "fd_manager.h"
#include <netinet/tcp.h>
namespace RPC {
static Logger::ptr logger = RPC_LOG_ROOT();
Socket::ptr Socket::CreateTCP(Address::ptr address) {
    return Socket::ptr (new Socket(address->getFamily(), TCP, IPPROTO_TCP));
}
Socket::ptr Socket::CreateUDP(Address::ptr address) {
    return Socket::ptr (new Socket(address->getFamily(), UDP, IPPROTO_UDP));
}


Socket::ptr Socket::CreateTCPSocket() {
    return Socket::ptr (new Socket(IPV4, TCP, IPPROTO_TCP));
}

Socket::ptr Socket::CreateUDPSocket() {
    return Socket::ptr(new Socket(IPV4, UDP, IPPROTO_UDP));
}

Socket::ptr Socket::CreateTCPSocket6() {
    return Socket::ptr(new Socket(IPV6, TCP, IPPROTO_TCP));
}

Socket::ptr Socket::CreateUDPSocket6() {
    return Socket::ptr(new Socket(IPV6, UDP, IPPROTO_UDP));
}

Socket::ptr Socket::CreateUnixTCPSocket() {
    return Socket::ptr(new Socket(UNIX, TCP, IPPROTO_TCP));
}

Socket::ptr Socket::CreateUnixUDPSocket() {
    return Socket::ptr(new Socket(UNIX, UDP, IPPROTO_UDP));
}  


Socket::Socket(int family, int type, int protocol):fd_(-1), family_(family), type_(type), protocol_(protocol), is_connected_(false){
    local_address_ = nullptr;
    remote_address_ = nullptr;
}
Socket::~Socket() {
    close();
}
bool Socket::setOption(int level, int optname, void *result, size_t len) {
    int rt = setsockopt(fd_, level, optname, result, (socklen_t)len);
    if (rt != 0) {
        RPC_LOG_ERROR(logger) << "Socket::setOption sock=" <<fd_ << " level=" << level << " optname=" << optname << " errno=" << errno << " errorstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::getOption(int level, int optname, void* result, size_t* len) {
    int rt = getsockopt(fd_, level, optname, result, (socklen_t *)len);
    if (rt != 0) {
        RPC_LOG_ERROR(logger) << "Socket::getOption sock=" << fd_ << " level=" << level << " optname=" << optname << " errno=" << errno << " errorstr=" << strerror(errno);
        return false;
    }
    return true;
}

void Socket::setSendTimeout(uint64_t time) {
    struct timeval tv = {int(time/1000), int(time % 1000 *1000)};;
    setOption(SOL_SOCKET, SO_SNDTIMEO, &tv);
}
uint64_t Socket::getSendTimeout() const {
    FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd_);
    if (!fdctx) {
        RPC_LOG_DEBUG(logger) << "fdContext is not exist";
        return -1;
    }
    return fdctx->getTimeout(SO_SNDTIMEO);
}

void Socket::newSocket() {
    fd_ = ::socket(family_, type_, protocol_);
    if (fd_ == -1) {
        RPC_LOG_ERROR(logger) << "create socket error, errno=" << errno; 
    } else {
        initSocket();
    }
}

void Socket::initSocket() {
    int val = 1;
    setOption(SOL_SOCKET, SO_REUSEADDR, &val);
    if (type_ == SOCK_STREAM) {
        setOption(IPPROTO_TCP, TCP_NODELAY, &val);
    }
}

bool Socket::isVaild() const {
    if (fd_ == -1) {
        return false;
    }
    RPC::FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd_);
    if(!fdctx || fdctx->isClosed() || !fdctx->isSocket()) {
        return false;
    }
    return true;
}

bool Socket::bind(const RPC::Address::ptr address) {
    if (!isVaild()) {
        newSocket();
        if (!isVaild()) {
            return false;
        }
    }
    if (address->getFamily() != family_) {
        RPC_LOG_ERROR(logger) << "sockt bind error, address family is not equal to socket family";
        return false;
    }

    int ret = ::bind(fd_, address->getSockAddr(), address->getSockAddrLen());
    if (ret == -1) {
        RPC_LOG_ERROR(logger) << "socket bind error, errno=" << errno;
        return false;
    }
    getLocalAddress();
    return true;
}
bool Socket::connect(const RPC::Address::ptr address, uint64_t timeout_ms) {
    if (!isVaild()) {
        newSocket();
        if (!isVaild()) {
            return false;
        }
    }
    if (address->getFamily() != family_) {
        RPC_LOG_WARN(logger) << "socket connect error, address family is not equal to socket family";
        return false;
    }
    if (timeout_ms == (uint64_t)-1) {
        int ret = ::connect(fd_, address->getSockAddr(), address->getSockAddrLen());
        if (ret == -1) {
            RPC_LOG_WARN(logger)  << "sock=" << fd_ << " connect(" << address->toString() << ") error, errno="
                                     << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    } else {
        int ret = ::connect_with_timeout(fd_, address->getSockAddr(), address->getSockAddrLen(), timeout_ms);
        if (ret == -1) {
            RPC_LOG_WARN(logger)  << "sock=" << fd_ << " connect(" << address->toString() << ") error, errno="
                                     << errno << " errstr=" << strerror(errno);
            close();
            return false;
        }
    }
    is_connected_ = true;
    getLocalAddress();
    getRemoteAddress();
    return true;

}
Socket::ptr Socket::accept() {
    int newsock = ::accept(fd_, nullptr, nullptr);
    if (newsock < 0) {
        RPC_LOG_WARN(logger) << "accept(" << fd_ << ") "
                << "errno=" << errno << "errstr=" << strerror(errno);
        return nullptr;
    }
    Socket::ptr sock = std::make_shared<Socket>(family_, type_, protocol_);
    if (sock->initSocketByFd(newsock)) {
        return sock;
    }
    return nullptr;
}

bool Socket::listen(int backlog) {
    if (!isVaild()) {
        RPC_LOG_ERROR(logger) << "listen error, sock = -1";
        return false;
    }
    int ret = ::listen(fd_, backlog);
    if (ret == -1) {
        RPC_LOG_ERROR(logger) << "listen error, errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Socket::close() {
    if (!is_connected_ && fd_ == -1) {
        return true;
    }
    is_connected_ = false;
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    return true;
}

int Socket::recv(void *buff, size_t length, int flag) {
    if (!is_connected_) {
        return -1;
    }
    return ::recv(fd_, buff, length, flag);
}

int Socket::recv(iovec *buff, size_t length, int flag) {
    if (!is_connected_) {
        return -1;
    }
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = buff;
    msg.msg_iovlen = length;
    return ::recvmsg(fd_, &msg, flag);
}
int Socket::send(const void *buff, size_t length, int flag) {
    if (!is_connected_) {
        return -1;
    }
    return ::send(fd_, buff, length, flag);
}

int Socket::send(const iovec *buff, size_t length, int flag) {
    if (!is_connected_) {
        return -1;
    }
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buff;
    msg.msg_iovlen = length;
    return ::sendmsg(fd_, &msg, flag);
}


int Socket::recvfrom(void *buf, size_t len, Address::ptr source, int flags) {
    if (!isVaild()) {
        return -1;
    }
    socklen_t socklen = source->getSockAddrLen();
    return ::recvfrom(fd_, buf, len, flags, source->getSockAddr(), &socklen);
}
int Socket::sendto(const void *buf, size_t len, const Address::ptr dest, int flags) {
    if (!isVaild()) {
        return -1;
    }
    return ::sendto(fd_, buf, len, flags, dest->getSockAddr(), dest->getSockAddrLen());
}


int Socket::recvfrom(iovec *buf, size_t len, Address::ptr source, int flags) {
    if (!isVaild()) {
        return -1;
    }
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = buf;
    msg.msg_iovlen = len;
    msg.msg_name = source->getSockAddr();
    msg.msg_namelen = source->getSockAddrLen();
    return ::recvmsg(fd_, &msg, flags);
}
int Socket::sendto(const iovec *buf, size_t len, const Address::ptr dest, int flags) {
    if (!isVaild()) {
        return -1;
    }
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = (iovec *)buf;
    msg.msg_iovlen = len;
    msg.msg_name = dest->getSockAddr();
    msg.msg_namelen = dest->getSockAddrLen();
    return ::sendmsg(fd_, &msg, flags);
}

bool Socket::initSocketByFd(int fd) {
    RPC::FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
    if (fdctx && fdctx->isSocket() && !fdctx->isClosed()) {
        fd_ = fd;
        is_connected_ = true;
        initSocket();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
}


Address::ptr Socket::getLocalAddress() {
    if (local_address_) {
        return local_address_;
    }
    Address::ptr result;
    switch (family_) {
        case AF_INET: {
            result.reset(new IPv4Address());
            break;
        }
        case AF_INET6: {
            result.reset(new IPv6Address());
            break;
        }
        case AF_UNIX: {
            result.reset(new UnixAddress());
            break;
        }
        default:
            result.reset(new UnknownAddress(family_));
    }
    socklen_t len = result->getSockAddrLen();
    /* 将当前socket的地址绑定到address上*/
    if (getsockname(fd_, result->getSockAddr(), &len)) {
        return Address::ptr (new UnknownAddress(family_));
    }

    if (family_ == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(len);
    }

    local_address_ = result;
    return result;
}

Address::ptr Socket::getRemoteAddress() {
    if (remote_address_) {
        return remote_address_;
    }
    Address::ptr result;
    switch (family_) {
        case AF_INET:
            result.reset(new IPv4Address());
            break;
        case AF_INET6:
            result.reset(new IPv6Address());
            break;
        case AF_UNIX:
            result.reset(new UnixAddress());
            break;
        default:
            result.reset(new UnknownAddress(family_));
            break;
    }
    socklen_t len = result->getSockAddrLen();
    /* 将与当前socket的连接的地址绑定到address上*/
    if (getpeername(fd_, result->getSockAddr(), &len)) {
        return Address::ptr(new UnknownAddress(family_));
    }

    if (family_ == AF_UNIX) {
        UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
        addr->setAddrLen(len);
    }

    remote_address_ = result;
    return result;
}


int Socket::getError() {
    int error = 0;
    size_t len = sizeof(error);
    if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    return error;
}

bool Socket::cancelRead() {
    return IOManager::GetThis()->cancelEvent(fd_, RPC::IOManager::Event::READ);
}
bool Socket::cancelWrite() {
    return IOManager::GetThis()->cancelEvent(fd_, RPC::IOManager::Event::WRITE);
}
bool Socket::cancelAccept() {
    return IOManager::GetThis()->cancelEvent(fd_, RPC::IOManager::Event::READ);
}
bool Socket::cancelAll() {
    return IOManager::GetThis()->cancelAllEvent(fd_);
}

std::ostream& Socket::dump(std::ostream& os) const {
    os << "[Socket sock=" << fd_ 
        << " is_connected=" << is_connected_
        << " family=" << family_
        << " type=" << type_
        << " protocol=" << protocol_;
    if (local_address_) {
        os << " local_address=" << local_address_->toString();
    }
    if (remote_address_) {
        os << " remote_address=" << remote_address_->toString();
    }
    os << "]";
    return os;
}


}