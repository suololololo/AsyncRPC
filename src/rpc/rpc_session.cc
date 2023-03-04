#include "rpc/rpc_session.h"
#include "rpc/protocol.h"
#include "log.h"
namespace RPC {
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
RPCSession::RPCSession(Socket::ptr socket, bool owner):SocketStream(socket, owner) {

}
std::shared_ptr<Protocol> RPCSession::recvRequest() {
    Protocol::ptr request(new Protocol);
    ByteArray::ptr ba(new ByteArray);
    if (readFixSize(ba, request->BASE_LENGTH) <= 0) {
        RPC_LOG_DEBUG(logger) << "lenth not enough";
        return nullptr;
    }
    ba->setPosition(0);
    request->decodeMeta(ba);
    /* magic + version + type + sequence id + content length */
    if (request->getMagic() != Protocol::MAGIC) {
        return nullptr;
    }
    if (request->getContentLength() == 0) {
        return request;
    }

    std::string buff;
    buff.resize(request->getContentLength());
    if (readFixSize(&buff[0], request->getContentLength()) <= 0) {
        RPC_LOG_INFO(logger) << "read content length, error";
        return nullptr;
    }

    request->setContent(buff);
    return request;
}

int RPCSession::sendResponse(std::shared_ptr<Protocol> response) {
    ByteArray::ptr ba = response->encode();
    MutexType::Lock lock(mutex_);
    return writeFixSize(ba, ba->getSize());
}

}