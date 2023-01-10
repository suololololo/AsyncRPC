#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__
#include "byte_array.h"

namespace RPC {
class Serializer {
public:
    typedef std::shared_ptr<Serializer> ptr;

    Serializer() {
        byte_array_ = std::make_shared<ByteArray>();
    } 

    Serializer(ByteArray::ptr byte_array):byte_array_(byte_array) {

    }
    ~Serializer() {

    }

    size_t getSize() { return byte_array_->getSize();}

    void reset() { byte_array_->setPosition(0); }

    /**
     * @brief 核心api 反序列化各种类型的数据
     * 
     */
    template <class T>
    void read(T &t) {
        if (std::is_same<T, bool>::value) {
            t = byte_array_->readFint8();
        } else if (std::is_same<T, float>::value) {
            t = byte_array_->readFloat();
        } else if (std::is_same<T, double>::value) {
            t = byte_array_->readDouble();
        } else if (std::is_same<T, int8_t>::value) {
            t = byte_array_->readFint8();
        } else if (std::is_same<T, uint8_t>::value) {
            t = byte_array_->readFuint8();
        } else if (std::is_same<T, int16_t>::value) {
            t = byte_array_->readFint16();
        } else if (std::is_same<T, uint16_t>::value) {
            t = byte_array_->readFuint16();
        } else if (std::is_same<T, int32_t>::value) {
            t = byte_array_->readInt32();
        } else if (std::is_same<T, uint32_t>::value) {
            t = byte_array_->readUint32();
        } else if (std::is_same<T, int64_t>::value) {
            t = byte_array_->readInt64();
        } else if (std::is_same<T, uint64_t>::value) {
            t = byte_array_->readUint64();
        } else if (std::is_same<T, std::string>::value) {
            t = byte_array_->readStringVint();
        }
    }

    /**
     * @brief 核心api 序列化各种类型的数据
     * 
     * @tparam T 
     * @param t 
     */
    template <class T>
    void write(T &t) {
        if (std::is_same<T, bool>::value) {
            byte_array_->writeFint8(t);
        } else if (std::is_same<T, float>::value) {
            byte_array_->writeFloat(t);
        } else if (std::is_same<T, double>::value) {
            byte_array_->writeDouble(t);
        } else if (std::is_same<T, int8_t>::value) {
            byte_array_->writeFint8(t);
        } else if (std::is_same<T, uint8_t>::value) {
            byte_array_->writeFuint8(t);
        } else if (std::is_same<T, int16_t>::value) {
            byte_array_->writeFint16(t);
        } else if (std::is_same<T, uint16_t>::value) {
            byte_array_->writeFuint16(t);
        } else if (std::is_same<T, int32_t>::value) {
            byte_array_->writeInt32(t);
        } else if (std::is_same<T, uint32_t>::value) {
            byte_array_->writeUint32(t);
        } else if (std::is_same<T, int64_t>::value) {
            byte_array_->writeInt64(t);
        } else if (std::is_same<T, uint64_t>::value) {
            byte_array_->writeUint64(t);
        } else if (std::is_same<T, std::string>::value) {
            byte_array_->writeStringVint(t);
        } else if (std::is_same<T, char*>::value) {
            byte_array_->writeStringVint(std::string(t));
        } else if (std::is_same<T, const char *>::value) {
            byte_array_->writeStringVint(std::string(t));
        }
    }


private:
    ByteArray::ptr byte_array_;

};


};


#endif