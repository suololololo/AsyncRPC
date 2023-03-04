#include "byte_array.h"
#include "utils.h"
#include "log.h"
#include <string.h>
#include <endian.h>
#include <math.h>
#include <iomanip>
namespace RPC {
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
static uint32_t EncodeZigzag32(const int32_t &value) {
    return (uint32_t)(value<< 1) ^ (value >> 31);
}
static uint64_t EncodeZigzag64(const int64_t &value) {
    return (uint64_t)(value << 1) ^(value >> 63);
}

static int32_t DecodeZigzag32(const uint32_t &value) {
    return (int32_t)(value >> 1) ^-(value &1);
}

static int64_t DecodeZigzag64(const uint64_t &value) {
    return (int64_t)(value >> 1) ^-(value &1);
}


ByteArray::ByteArray(size_t size):head_(new Node(size)), curr_(head_), base_size_(size), position_(0), 
    capacity_(size), size_(0), endian_(RPC_LITTLE_ENDIAN){

}
ByteArray::~ByteArray() {
        Node* temp = head_;
        while(temp) {
            curr_ = temp;
            temp = temp->next;
            delete curr_;
        }
    }

ByteArray::Node::Node():ptr(nullptr), next(nullptr), size(0) {
}        
ByteArray::Node::Node(size_t s):ptr(new char[s]), next(nullptr), size(s){

}
ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
}

void ByteArray::addCapacity(size_t size) {
        if (size <= 0)
            return;
        size_t old_capacity = getCapacity();
        if (old_capacity >= size) 
            return;
        size = size - old_capacity;
        size_t count = ceil(1.0 * size / base_size_);
        Node* temp = head_;
        while(temp->next) {
            temp = temp->next;
        }
        Node* first = NULL;
        for (size_t i = 0; i < count; ++i) {
            temp->next = new Node(base_size_);
            if (first == NULL)
                first = temp->next;
            temp = temp->next;
            capacity_ += base_size_;
        }

        if (old_capacity == 0) {
            curr_ = first;
        }
        
}

void ByteArray::write(const void *buf, size_t size) {
    if (size <= 0)
        return;
    addCapacity(size);

    size_t npos = position_ % base_size_;

    //当前node剩余大小
    size_t ncap = curr_->size - npos;

    // buffer 的偏移量
    size_t bpos = 0;
    while (size > 0) {
        if (ncap >= size) {
            memcpy(curr_->ptr + npos, (const char *)buf + bpos, size);
            if (curr_->size == (size + npos)) {
                curr_ = curr_->next;
            }
            position_ += size;
            bpos += size;
            size = 0;
        } else {
            memcpy(curr_->ptr + npos, (const char *)buf + bpos, ncap);
            curr_ = curr_->next;
            position_ += ncap;
            bpos += ncap;
            size -= ncap;

            npos = 0;
            ncap = curr_->size;
        }
    }

    // 修改数据大小记录
    if (position_ > size_) {
        size_ = position_;
    }


}
void ByteArray::read(void *buf, size_t size) {
    if (size <= 0)
        return;
    size_t npos = position_ % base_size_;

    size_t ncap = curr_->size - npos;

    size_t bpos = 0;

    while (size > 0) {
        if (ncap >= size) {
            memcpy((char*)buf + bpos, curr_->ptr + npos, size);
            if (curr_->size == (npos + size)) {
                curr_ = curr_->next;
            }
            position_ += size;
            
            bpos += size;
            size = 0;
        } else {
            memcpy((char *)buf + bpos, curr_->ptr + npos, ncap);
            curr_ = curr_->next;
            position_ += ncap;
            bpos += ncap;

            size -= ncap;
            npos = 0;
            ncap = curr_->size;
        }
    }

}

void ByteArray::read(void *buf, size_t size, size_t position) const {
    if (size > (size_ - position)) {
        RPC_LOG_ERROR(logger) << "not enough len to read";
        return;
    }

    Node* curr = curr_;
    size_t npos = position % base_size_;
    size_t ncap = curr->size - npos;
    size_t bpos = 0;
    while (size > 0) {
        if (ncap >= size) {
            memcpy((char *)buf + bpos, curr->ptr+ npos, size);
            if (curr->size == (npos + size)) {
                curr = curr->next;
            }
            bpos += size;
            position += size;
            size = 0;
        } else {
            memcpy((char *)buf + bpos,  curr->ptr + npos, ncap);
            bpos += ncap;
            position += ncap;

            npos = 0;
            size -= ncap;

            curr = curr->next;
            ncap = curr->size;
        }
    }
}


void ByteArray::writeFint8(int8_t value){
    write(&value, sizeof(value));
}

void ByteArray::writeFint16(int16_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value) {
    write(&value, sizeof(value));
}
void ByteArray::writeFint64(int64_t value) {
    write(&value, sizeof(value));
}
void ByteArray::writeFuint8(uint8_t value) {
    write(&value, sizeof(value));
}
void ByteArray::writeFuint16(uint16_t value) {
    write(&value, sizeof(value));
}
void ByteArray::writeFuint32(uint32_t value) {
    write(&value, sizeof(value));
}
void ByteArray::writeFuint64(uint64_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeInt32(int32_t value) {
    writeUint32(EncodeZigzag32(value));
}

void ByteArray::writeUint32(uint32_t value) {
    uint8_t temp[5];
    uint8_t i = 0;
    while (value >= 0x80) {
        // 取低7位 并判断是否后续还有数据
        temp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    temp[i++] = value;
    write(temp, i);
}
    
void ByteArray::writeInt64(int64_t value) {
    writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
    uint8_t temp[10];
    uint8_t i = 0;
    while (value >= 0x80) {
        temp[i++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    temp[i++] = value;
    write(temp, i);
}


int8_t ByteArray::readFint8() {
    int8_t value;
    read(&value, sizeof(value));
    return value;
}
uint8_t ByteArray::readFuint8() {
    uint8_t value;
    read(&value, sizeof(value));
    return value;
}

#define XX(type) \
        type v; \
        read(&v, sizeof(v)); \
        if (endian_ == RPC_BYTE_ORDER) {\
            return v; \
        } else { \
            return ByteSwap(v);\
        }


int16_t ByteArray::readFint16() {
    XX(int16_t); 
}
int32_t ByteArray::readFint32() {
    XX(int32_t);
}
int64_t ByteArray::readFint64() {
    XX(int64_t);
}

uint16_t ByteArray::readFuint16() {
    XX(uint16_t);
}
uint32_t ByteArray::readFuint32() {
    XX(uint32_t);
}
uint64_t ByteArray::readFuint64() {
    XX(uint64_t);
}

#undef XX

int32_t ByteArray::readInt32() {
    return DecodeZigzag32(readUint32());
}
uint32_t ByteArray::readUint32() {
    uint32_t value = 0;
    for (int i = 0; i < 32; i += 7) {
        uint8_t temp = readFuint8();
        if (temp < 0x80) {
            value |= (((uint32_t)temp) << i);
            break;
        } else {
            value |= (((uint32_t)(temp & 0x7F)) << i);
        }
    }
    return value;
}
int64_t ByteArray::readInt64() {
    return DecodeZigzag64(readUint64());
}
uint64_t ByteArray::readUint64() {
    uint64_t value = 0;
    for (int i = 0; i < 64; i += 7) {
        uint8_t temp = readFuint8();
        if (temp < 0x80) {
            value |= (((uint64_t) temp) << i);
            break;
        } else {
            value |= ((uint64_t)(temp & 0x7F) << i);
        }
    }
    return value;
}



void ByteArray::writeFloat(float value) {
    uint32_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint32(v);
}

void ByteArray::writeDouble(double value) {
    uint64_t v;
    memcpy(&v, &value, sizeof(value));
    writeFuint64(v);
}

    /**
     * @brief 用uint16_t类型记录string长度， 并写入
     * 
     * @param value 
     */
void ByteArray::writeStringF16(const std::string &value) {
    writeFuint16(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF32(const std::string &value) {
    writeFuint32(value.size());
    write(value.c_str(), value.size());
}

void ByteArray::writeStringF64(const std::string &value) {
    writeFuint64(value.size());
    write(value.c_str(), value.size());
}

/**
 * @brief 用varint64记录string长度，并写入
 * 
 * @param value 
 */
void ByteArray::writeStringVint(const std::string &value) {
    writeUint64(value.size());
    write(value.c_str(), value.size());
}

    /**
     * @brief 不记录长度写入
     * 
     * @param value 
     */
void ByteArray::writeStringWithoutLength(const std::string &value) {
    write(value.c_str(), value.size());
}

float ByteArray::readFloat() {
    uint32_t v = readFuint32();
    float value;
    memcpy(&value, &v, sizeof(value));
    return value;
}

double ByteArray::readDouble() {
    uint64_t v = readFuint64();
    double value;
    memcpy(&value, &v, sizeof(value));
    return value;
}
std::string ByteArray::readStringF16() {
    uint16_t len = readFuint16();
    std::string value;
    value.resize(len);
    read(&value[0], len);
    return value;
}
std::string ByteArray::readStringF32() {
    uint32_t len = readFuint32();
    std::string value;
    value.resize(len);
    read(&value[0], len);
    return value;
}
std::string ByteArray::readStringF64() {
    uint64_t len = readFuint64();
    std::string value;
    value.resize(len);
    read(&value[0], len);
    return value;
}
std::string ByteArray::readStringVint() {
    uint64_t len = readUint64();
    std::string value;
    value.resize(len);
    read(&value[0], len);
    return value;
}



void ByteArray::clear() {
    position_ = size_ = 0;
    capacity_ = base_size_;
    Node *temp = head_->next;
    while (temp) {
        curr_ = temp;
        temp = temp->next;
        delete curr_;
    }
    curr_ = head_;
    head_->next = nullptr;

}

void ByteArray::setPosition(size_t v) {
    if (v > capacity_) {
        throw std::out_of_range("set position out of range");
    }
    if (v < 0)
        return;
    position_ = v;
    if (position_ > size_) {
        size_ = position_;
    }

    curr_ = head_;
    while ( v > curr_->size) {
        v -= curr_->size;
        curr_ = curr_->next;
    }
    if (v == curr_->size) {
        curr_ = curr_->next;
    }
}

bool ByteArray::writeToFile(const std::string &name) const {
    std::ofstream ofs;
    ofs.open(name, std::ios::trunc | std::ios::binary);
    if (!ofs) {
        RPC_LOG_ERROR(logger) << "write to file name = " << name << "error, error" 
            << errno << "error str = " << strerror(errno);
        return false;
    }
    size_t read_size = getReadableSize();
    size_t pos = position_; 
    Node* curr = curr_;
    while (read_size > 0) {
        size_t diff = pos % base_size_;
        size_t len = (read_size > base_size_ ? base_size_ : read_size) - diff;
        ofs.write(curr->ptr + diff, len);
        curr = curr->next;
        read_size -= len;
        pos += len;
    }

    return true;
}

bool ByteArray::readFromFile(const std::string &name) {
    std::ifstream ifs;
    ifs.open(name, std::ios::binary);
    if (!ifs) {
        RPC_LOG_ERROR(logger)  << "read from file name = " << name << "error, error" 
            << errno << "error str = " << strerror(errno);
        return false;
    }
    std::shared_ptr<char> buff(new char[base_size_], [](char *ptr) {delete[] ptr;});
    
    while(!ifs.eof()) {
        ifs.read(buff.get(), base_size_);
        write(buff.get(), ifs.gcount());
    }
    return true;    
}

std::string ByteArray::toString() const {
    std::string str;
    str.resize(getReadableSize());
    if (str.empty()) {
        return str;
    }

    read(&str[0], str.size(), position_);
    return str;
}

std::string ByteArray::toHexString() const {
    std::string str = toString();
    std::stringstream ss;
    for (size_t i = 0; i < str.size(); ++i) {
        if (i > 0 && i % 32 ==0) {
            ss << std::endl;
        }
        ss << std::setw(2) << std::setfill('0') << std::hex << (int)(uint8_t)str[i] << " ";
    }
    return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec> &buffers, uint64_t len) const {
    if (getReadableSize() < len) {
        len = getReadableSize();
    }
    if (len == 0) {
        return 0;
    }
    uint64_t size = len;

    size_t npos = position_ % base_size_;
    size_t ncap = curr_->size - npos;
    Node* curr = curr_;

    struct iovec iov;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = len;
            
            len = 0;
            npos += len; 
        } else {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = ncap;


            len -= ncap;
            npos = 0;
            curr = curr->next;
            ncap = curr->size;

        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec> &buffers, uint64_t len, size_t position) const {
    if (len > getReadableSize())
        len = getReadableSize();
    if (len == 0) 
        return 0;
    
    uint64_t size = len;
    size_t npos = position % base_size_;
    size_t ncap = curr_->size - npos;
    Node* curr = curr_;
    struct iovec iov;
    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = len;

            len = 0;
            npos += len;
        } else {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = ncap;

            npos = 0;
            len -= ncap;
            curr = curr->next;
            ncap = curr->size;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec> &buffers, uint64_t len) {
    if (len == 0) {
        return 0;
    }
    addCapacity(len);
    uint64_t size = len;
    size_t npos = position_ % base_size_;
    size_t ncap = curr_->size - npos;
    Node* curr = curr_;
    struct iovec iov;

    while (len > 0) {
        if (ncap >= len) {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = len;

            len = 0;
            npos += len;
            ncap += len;
        } else {
            iov.iov_base = curr->ptr + npos;
            iov.iov_len = ncap;
            len -= ncap;

            curr = curr->next;
            ncap = curr->size;
            npos = 0;
        }
        buffers.push_back(iov);
    }
    return size;

}

}
