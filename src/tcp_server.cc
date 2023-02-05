#include "tcp_server.h"
#include "log.h"
namespace RPC {

static RPC::Logger::ptr logger = RPC_LOG_ROOT();


TCPServer::TCPServer(RPC::IOManager* worker, IOManager *accpetWorker)
    :worker_(worker),
    acceptWorker_(accpetWorker),
    stop_(true)
{

}
TCPServer::~TCPServer() {
    // stop();
}

bool TCPServer::bind(RPC::Address::ptr addr) {
    std::vector<RPC::Address::ptr> addrs;
    addrs.push_back(addr);
    std::vector<RPC::Address::ptr> fail;
    return bind(addrs, fail);
}
bool TCPServer::bind(const std::vector<RPC::Address::ptr> &addrs, std::vector<RPC::Address::ptr> &fail) {
    for (auto &addr : addrs) {
        Socket::ptr sock = Socket::CreateTCP(addr);
        if (!sock->bind(addr)) {
            RPC_LOG_ERROR(logger) << "bind fail, errno =" << errno 
            << " errstr=" << strerror(errno) << " addr=[" << addr->toString() << "]"; 
            fail.push_back(addr);
            continue; 
        }
        if (!sock->listen()) {
            RPC_LOG_ERROR(logger) << "listen fail, errno =" << errno 
            << " errstr=" << strerror(errno) << " addr=[" << addr->toString() << "]";
            fail.push_back(addr);
            continue;
        }
        socks_.push_back(sock);
    }
    for (auto & i : socks_) {
        RPC_LOG_INFO(logger) << "bind success, socket=" << *i;
    }
    if (!fail.empty()) {
        socks_.clear();
        return false;
    }

    return true;
}


void TCPServer::startAccept(Socket::ptr sock) {
    while (!stop_) {
        Socket::ptr client = sock->accept();
        if (client) {
            RPC_LOG_DEBUG(logger) << "accept sucess, socket=" << *client;
            worker_->Submit(std::bind(&TCPServer::handleClient, shared_from_this(), client));
        } else {
            RPC_LOG_ERROR(logger) << "accept error, errno=" << errno << " errstr=" 
            << strerror(errno);
        }
    }
}

bool TCPServer::start() {
    if (!stop_) {
        return true;
    }
    stop_ = false;
    for (auto &i : socks_) {
        acceptWorker_->Submit(std::bind(&TCPServer::startAccept, shared_from_this(), i));
    }
    return true;
}

bool TCPServer::stop() {
    if (stop_) {
        return true;
    }
    stop_ = true;
    auto self = shared_from_this();
    acceptWorker_->Submit([this, self]() {
        for (auto &i : socks_) {
            i->cancelAll();
            i->close();
        }
        socks_.clear();
    });
    return true;
}

void TCPServer::handleClient(Socket::ptr client) {
    RPC_LOG_DEBUG(logger) << *client;
}

}