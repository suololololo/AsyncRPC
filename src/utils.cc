#include "utils.h"
#include "fiber.h"
#include <unistd.h>
#include <syscall.h>
namespace RPC {
pid_t GetThreadId() {
    return syscall(SYS_gettid);
}


uint64_t GetFiberId() {
    return Fiber::GetFiberId();
}
}

