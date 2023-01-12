#ifndef __BYTE_ARRAY_H__
#define __BYTE_ARRAY_H__
#include <memory>
#include <stdint.h>
#include <sys/uio.h>
#include <vector>
namespace RPC {
/**
 * @brief 序列化模块定义
 * 
 */

class ByteArray {
public:
    typedef std::shared_ptr<ByteArray> ptr;
    
    struct Node {
        /**
         * @brief 内存节点定义
         * 
         */
        Node();
        Node(size_t size);
        ~Node();
        // 内存块地址指针
        char *ptr;
        Node* next;
        size_t size;
    };

    ByteArray(size_t size = 4096);
    ~ByteArray();
    /**
     * @brief 写入固定长度int8 数据
     * 
     * @param value 
     */
    void writeFint8(int8_t value);

    void writeFint16(int16_t value);

    void writeFint32(int32_t value);

    void writeFint64(int64_t value);

    void writeFuint8(uint8_t value);

    void writeFuint16(uint16_t value);

    void writeFuint32(uint32_t value);

    void writeFuint64(uint64_t value);
    /**
     * @brief 用变长的方式写入int32_t数据 需要进行编码
     * 
     * @param value 
     */
    void writeInt32(int32_t value);

    void writeUint32(uint32_t value);
    
    void writeInt64(int64_t value);

    void writeUint64(uint64_t value);

    int8_t readFint8();

    int16_t readFint16();

    int32_t readFint32();

    int64_t readFint64();

    uint8_t readFuint8();

    uint16_t readFuint16();

    uint32_t readFuint32();

    uint64_t readFuint64();

    int32_t readInt32();

    uint32_t readUint32();

    int64_t readInt64();

    uint64_t readUint64();

    void writeFloat(float value);

    void writeDouble(double value);

    /**
     * @brief 用uint16_t类型记录string长度， 并写入
     * 
     * @param value 
     */
    void writeStringF16(const std::string &value);

    void writeStringF32(const std::string &value);

    void writeStringF64(const std::string &value);

    /**
     * @brief 用varint64记录string长度，并写入
     * 
     * @param value 
     */
    void writeStringVint(const std::string &value);

    /**
     * @brief 不记录长度写入
     * 
     * @param value 
     */
    void writeStringWithoutLength(const std::string &value);


    float readFloat();

    double readDouble();

    std::string readStringF16();

    std::string readStringF32();

    std::string readStringF64();

    std::string readStringVint();

    /**
     * @brief 把ByteArray中的数据写入到文件中
     * 
     */

    bool writeToFile(const std::string &name) const;

    /**
     * @brief 从文件中读取数据
     * 
     * @param name 
     * @return true 
     * @return false 
     */
    bool readFromFile(const std::string &name);

    /**
     * @brief 清空ByteArray
     * 
     */
    void clear();
    /**
     * @brief 设置当前偏移位置
     * 
     * @param v 
     */
    void setPosition(size_t v);
    
    /**
     * @brief 把[position_, size_] 之间的数据转成string
     * 
     * @return std::string 
     */
    std::string toString() const;

    /**
     * @brief 把[postion_, size_] 之间的数据转成16进制string
     * 
     * @return std::string 
     */
    std::string toHexString() const;

    /**
     * @brief 将可读的缓存数据，存入buffers数组
     * 如果len > getReadableSize(),  len = getReadableSize()
     * @return 返回读取到的长度
     */
    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len = ~0ull) const;

    uint64_t getReadBuffers(std::vector<iovec> &buffers, uint64_t len, size_t position) const;
    
    /**
     * @brief 将可写的缓存数据存入buffers
     * 容量不足于写入len长度时，扩容
     * @return uint64_t 可写入长度
     */
    uint64_t getWriteBuffers(std::vector<iovec> &buffers, uint64_t len);
    /**
     * @brief 获取数据长度
     * 
     * @return size_t 
     */
    size_t getSize() const {return size_;}

    size_t getBaseSize() const { return base_size_;}

    size_t getReadableSize() const { return size_ - position_;}
    
    size_t getPosition() const { return position_; }
        /**
     * @brief 核心API，将buf中的数据写入指定内存块，写入大小为size
     * 
     * @param buf 
     * @param size 
     */
    void write(const void *buf, size_t size);
    
    /**
     * @brief 核心API，将内存块的数据读到 buf
     * 
     * @param buf 
     * @param size 
     */
    void read(void *buf, size_t size);
private:

    void read(void *buf, size_t size, size_t position) const;

    /**
     * @brief 将容量扩大至能够写下size大小的数据
     * 
     * @param size 
     */
    void addCapacity(size_t size);

    size_t getCapacity() const {return capacity_ - position_;}
private:
    //内存节点列表
    Node* head_;
    Node* curr_;

    //内存节点的基本大小
    size_t base_size_;
    //当前偏移位置
    size_t position_;
    //当前总容量
    size_t capacity_;
    // 当前数据大小
    size_t size_;
    // 字节序
    int8_t endian_;
};

}



#endif