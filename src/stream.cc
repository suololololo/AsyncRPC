#include "stream.h"

namespace RPC {
int Stream::readFixSize(void *buff, size_t len) {
    size_t left = len;
    size_t offset = 0;
    while (left > 0) {
        size_t l = read((char *)buff + offset, left);
        if (l <= 0) {
            return l;
        }
        offset += l;
        left -= l;
    }
    return len;
}
int Stream::readFixSize(ByteArray::ptr ba, size_t len) {
    size_t left = len;
    while (left > 0) {
        size_t l = read(ba, left);
        if (l <= 0) {
            return l;
        }
        left -= l;
    }
    return len;
}
int Stream::writeFixSize(const void *buff, size_t len) {
    size_t left = len;
    size_t offset = 0;
    while (left > 0) {
        size_t l = write((const char *)buff + offset, left);
        if (l <= 0) {
            return l;
        }
        offset += l;
        left -= l;
    }
    return len;
}
int Stream::writeFixSize(ByteArray::ptr ba, size_t len) {
    size_t left = len;
    while (left > 0) {
        size_t l = write(ba, left);
        if (l <= 0) {
            return l;
        }
        left -= l;
    }
    return len;
}

}