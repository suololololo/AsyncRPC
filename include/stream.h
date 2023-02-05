#ifndef __STREAM_H__
#define __STREAM_H__
#include "byte_array.h"
namespace RPC {
class Stream {
public:
    typedef std::shared_ptr<Stream> ptr;
    virtual ~Stream();
    virtual int readFixSize(void *buff, size_t len);
    virtual int readFixSize(ByteArray::ptr ba, size_t len);
    virtual int writeFixSize(const void *buff, size_t len);
    virtual int writeFixSize(ByteArray::ptr ba, size_t len);
    virtual int read(void *buff, size_t len) = 0;
    virtual int read(ByteArray::ptr ba, size_t len) = 0;
    virtual int write(const void *buff, size_t len) = 0;
    virtual int write(ByteArray::ptr ba, size_t len) = 0;
    virtual void close() = 0;
};
}



#endif