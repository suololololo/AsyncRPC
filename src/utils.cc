#include "utils.h"
#include <unistd.h>
#include <syscall.h>
namespace RPC {
pid_t GetThreadId() {
    return syscall(SYS_gettid);
}
}

