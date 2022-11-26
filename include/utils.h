#ifndef __UTILS_H__
#define __UTILS_H__
#include <pthread.h>
#include <stdint.h>
namespace RPC {
pid_t GetThreadId();

uint64_t GetFiberId();
}



#endif