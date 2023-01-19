#include "utils.h"
#include "fiber.h"
#include <unistd.h>
#include <syscall.h>
#include <sys/time.h>
namespace RPC {
pid_t GetThreadId() {
    return syscall(SYS_gettid);
}


uint64_t GetFiberId() {
    return Fiber::GetFiberId();
}

/**
 * @brief 获取当前时间的毫秒数 (ms)
 *  1秒= 1000 毫秒  1毫秒= 1000微秒
 * @return uint64_t 
 */
uint64_t GetCurrentMS() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec * 1000ul + tm.tv_usec / 1000;
}
/**
 * @brief 获取当前时间的微秒数(us)
 *  1秒= 1000 毫秒  1毫秒= 1000微秒
 * @return uint64_t 
 */
uint64_t GetCurrentUS() {
    struct timeval tm;
    gettimeofday(&tm, 0);
    return tm.tv_sec * 1000ul * 1000ul + tm.tv_usec;
}

}

