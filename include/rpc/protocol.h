#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__
#include "byte_array.h"
#include <string>
#include <sstream>
namespace RPC {
/**
 * @brief refer to https://github.com/zavier-wong/acid/tree/0f45acff75979d4636d217b78cd61fc2b1c01751
 * |Fuint8|Fuint8|Fuint8| Fuint32   |Fuint32|
 * |    |   |   |   |   |   |   |   |   |   |   | 
 * magic + version + type + sequence id + content length
 * 
 */

class Protocol {
public:
    typedef std::shared_ptr<Protocol> ptr;
    static const uint8_t MAGIC = 0xaa;
    static const uint8_t VERSION = 0x01;
    static const uint8_t BASE_LENGTH = 11;
    /**
     * @brief 消息类型
     * 
     */
    enum class MsgType :uint8_t {
        HEARTBEAT_PACKET,  //心跳包
        RPC_REQUEST,       //RPC 通用请求包
        RPC_RESPONSE,      //RPC 通用响应包
        RPC_METHOD_REQUEST,//RPC 方法请求调用
        RPC_METHOD_RESPONSE,//RPC 方法响应

        RPC_SERVICE_DISCOVER, //RPC 服务发现请求
        RPC_SERVICE_DISCOVER_RESPONSE, //RPC 服务发现响应
        RPC_SERVICE_REGISTER, // 服务注册请求
        RPC_SERVICE_REGISTER_RESPONSE, // 服务注册请求响应

        RPC_SUBSCRIBE_REQUEST,  //订阅请求
        RPC_SUBSCRIBE_RESPONSE, // 订阅响应

        RPC_PUBLISH_REQUEST, // 发布请求
        RPC_PUBLISH_RESPONSE, // 发布响应
        RPC_PROVIDER,         // 向注册中心声明为RPC服务提供方
        RPC_CONSUMER,         // 向注册中心声明为RPC服务消费方
    };

    static Protocol::ptr Create(MsgType type,  const std::string &content, uint32_t id = 0) {
        Protocol::ptr res = std::make_shared<Protocol>();
        res->setMsgType(type);
        res->setContent(content);
        res->setSequenceId(id);
        res->setContentLength(content.size());
        return res;
    }

    static Protocol::ptr HeartBeat() {
        Protocol::ptr heartbeat = Create(Protocol::MsgType::HEARTBEAT_PACKET, "");
        return heartbeat;
    }

public:
    ByteArray::ptr encodeMeta() {
        ByteArray::ptr bt = std::make_shared<ByteArray>();
        bt->writeFuint8(magic_);
        bt->writeFuint8(version_);
        bt->writeFuint8(type_);
        bt->writeFuint32(sequence_id_);
        bt->writeFuint32(content_.size());
        bt->setPosition(0);
        return bt;
    }
    void decodeMeta(ByteArray::ptr bt) {
        magic_ = bt->readFuint8();
        version_ = bt->readFuint8();
        type_ = bt->readFuint8();   
        sequence_id_ = bt->readFuint32();
        content_length_ = bt->readFuint32();
    }

    ByteArray::ptr encode() {
        ByteArray::ptr bt = std::make_shared<ByteArray>();
        bt->writeFuint8(magic_);
        bt->writeFuint8(version_);
        bt->writeFuint8(type_);
        bt->writeFuint32(sequence_id_);
        bt->writeStringF32(content_);
        bt->setPosition(0);
        return bt;
    }
    void decode(ByteArray::ptr bt) {
        magic_ = bt->readFuint8();
        version_ = bt->readFuint8();
        type_ = bt->readFuint8();   
        sequence_id_ = bt->readFuint32();
        content_ = bt->readStringF32();
        content_length_ = content_.size();
    }

    void setMagic(uint8_t magic) { magic_ = magic;}
    void setVersion(uint8_t version) { version_ = version;}
    void setMsgType(MsgType type) { type_ = static_cast<uint8_t>(type);}
    void setContent(const std::string &content) { content_ = content;}
    void setSequenceId(uint32_t id) { sequence_id_ = id;}
    void setContentLength(uint32_t len) { content_length_ = len;}
    
    uint8_t getMagic() const { return magic_;}
    uint8_t getVersion() const { return version_;}
    MsgType getMsgType() const { return static_cast<MsgType>(type_);}
    uint32_t getSequenceId() const { return sequence_id_;}
    uint32_t getContentLength() const { return content_length_;}
    const std::string &getContent() const { return content_;}
    std::string toString() const {
        std::stringstream ss;
        ss << "[ magic=" << magic_
            << " version=" << version_
            << " type=" << type_
            << " id=" << sequence_id_
            << " length=" << content_length_
            << " content=" << content_
            << " ]";
        return ss.str();
    }
private:
    uint8_t magic_ = MAGIC;
    uint8_t version_ = VERSION;
    uint8_t type_ = 0;
    uint32_t sequence_id_ = 0;
    uint32_t content_length_ = 0;
    std::string content_;
};


};



#endif