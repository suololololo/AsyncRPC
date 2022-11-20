#ifndef __MACRO_H__
#define __MACRO_H__
#include <assert.h>
#include <iostream>
#if defined(__GNUC__) || defined(__llvm__)
# define RPC_LIKELY(x) __builtin_expect(!!(x), 1)
# define RPC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else 
# define RPC_LIKELY(x) (x)
# define RPC_UNLIKELY(x) (x) 
#endif

#define RPC_ASSERT(x) \
if (RPC_UNLIKELY(!(x))) { \
    assert(x);\
    exit(1);\
}

#define RPC_ASSERT2(x, str) \
if (RPC_UNLIKELY(!(x))) { \
    std::cout << str << std::endl;\
    assert(x);\
    exit(1);\
}




#endif