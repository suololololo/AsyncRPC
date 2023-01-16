#ifndef __UTILS_H__
#define __UTILS_H__

#define RPC_LITTLE_ENDIAN 1
#define RPC_BIG_ENDIAN 2

#include <pthread.h>
#include <stdint.h>
#include <byteswap.h>
#include <type_traits>



namespace RPC {
pid_t GetThreadId();

uint64_t GetFiberId();

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type
ByteSwap(T value) {
    return (T)bswap_64((uint64_t)value);
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
ByteSwap(T value) {
    return (T)bswap_32((uint32_t)value);
}


template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
ByteSwap(T value) { 
    return (T)bswap_16((uint16_t)value);
}
#if BYTE_ORDER == BIG_ENDIAN
#define RPC_BYTE_ORDER RPC_BIG_ENDIAN
#else 
#define RPC_BYTE_ORDER RPC_LITTLE_ENDIAN
#endif

template<class T>
T EndianCast(T value) {
    if (sizeof(T) == sizeof(uint8_t) || RPC_BYTE_ORDER == RPC_BIG_ENDIAN) {
        return value;
    } else {
        return ByteSwap(value);
    }
}

}



#endif