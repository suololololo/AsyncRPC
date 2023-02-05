#include "hook.h"
#include "log.h"
#include "fiber.h"
#include "io_manager.h"
#include "fd_manager.h"
#include "macro.h"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/ioctl.h>
static RPC::Logger::ptr logger = RPC_LOG_ROOT();
namespace RPC {

static thread_local bool t_hook_enable = false;
#define HOOK_FUN(XX) \
    XX(sleep)\
    XX(usleep)\
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(recv) \
    XX(recvfrom) \
    XX(read) \
    XX(readv) \
    XX(recvmsg) \
    XX(sendmsg) \
    XX(sendto) \
    XX(send) \
    XX(write) \
    XX(writev) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

/**
 * @brief 把所有需要hook的系统函数hook上
 * 
 */
void hook_init() {
    static bool is_init = false;
    if (is_init) {
        return;
    }
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name);
    //标准库实现的函数指针
    HOOK_FUN(XX)
#undef XX
}

struct _HookIniter{
    _HookIniter() {
        hook_init();
    }
};
static _HookIniter hookiniter;
static uint64_t s_connect_timeout = -1;
bool is_enable_hook() {
    return t_hook_enable;
}

void set_hook_enable(bool flag) {
    t_hook_enable = flag;
}

}

template<typename OriginFun, typename...Args>
static ssize_t do_io(int fd, RPC::IOManager::Event event, const char *fun_name, OriginFun fun, Args&&...args) {
    if (!RPC::is_enable_hook()) {
        return fun(fd, std::forward<Args>(args)...);
    }
    RPC::FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
    if (!fdctx) {
        // 不存在fd上下文(不是socket)
        return fun(fd, std::forward<Args>(args)...);
    }
    if (fdctx->isClosed()) {
        // fd 已经关闭
        /* Bad file number */
        errno = EBADF;
        return -1;
    }
    if (!fdctx->isSocket() || fdctx->userNonblock()) {
        // 只处理socket和系统级阻塞的情况
        return fun(fd, std::forward<Args>(args)...);
    }

    // 获取超时时间
    uint64_t timeout = 0;
    std::shared_ptr<int> timecondition(new int{0});
    if (event == RPC::IOManager::READ) {
        timeout = fdctx->getTimeout(SO_RCVTIMEO);
    } else {
        timeout = fdctx->getTimeout(SO_SNDTIMEO);
    }
retry:
    // 调用原始io函数
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
        // 系统中断
        n = fun(fd, std::forward<Args>(args)...);
    }
    if (n == -1 && errno == EAGAIN) {
        // 数据没准备好 挂起协程
        RPC::IOManager* iomanager = RPC::IOManager::GetThis();
        std::weak_ptr<int> weak_cond(timecondition);
        RPC::Timer::ptr timer;
        if (timeout != (uint64_t)-1) {
            // 有设置超时时间
            /* 过了超时时间，weak_cond指针所指对象仍然存在，触发回调函数 */
            timer = iomanager->addConditionTimer(timeout, [weak_cond, iomanager, fd, event]() {
                auto t = weak_cond.lock();
                if (!t || *t) {
                    // 条件已经不成立
                    return;
                }
                /* Connection timed out */
                *t = ETIMEDOUT;
                iomanager->cancelEvent(fd, event);
            }, weak_cond);
        }
        bool rt = iomanager->addEvent(fd, event);
        if (!rt) {
            /* 添加事件失败*/
            RPC_LOG_ERROR(logger) << "do_io add event error";
            if (timer) {
                timer->cancel();
            }
            return -1;
        }
        RPC::Fiber::YieldToHold();
        // 数据已经准备好 
        if (timer) {
            timer->cancel();
        }
        if(*timecondition){
            // io超时
            errno = *timecondition;
            return -1;
        }
        goto retry;
    }
    return n;
}

extern "C" {
#define XX(name) name ## _fun name ## _f = nullptr;
    HOOK_FUN(XX)
#undef XX
unsigned int sleep(unsigned int seconds) {
    if (!RPC::is_enable_hook()) {
        return sleep_f(seconds);
    } 
    RPC::IOManager* iomanager = RPC::IOManager::GetThis();
    RPC::Fiber::ptr fiber = RPC::Fiber::GetThis();

    RPC_ASSERT2(iomanager, "iomanager is not start");
    iomanager->addTimer(seconds * 1000, [iomanager, fiber]() mutable{
        iomanager->Submit(fiber, -1);
    });
    RPC::Fiber::YieldToHold();
    return 0;
}

int usleep(useconds_t usec) {
    if (!RPC::is_enable_hook()) {
        return usleep_f(usec);
    }
    RPC::IOManager* iomanger = RPC::IOManager::GetThis();
    RPC::Fiber::ptr fiber = RPC::Fiber::GetThis();
    RPC_ASSERT2(iomanger, "iomanager is not start");
    iomanger->addTimer(usec /1000, [iomanger, fiber]() mutable {
        iomanger->Submit(fiber, -1);
    });
    RPC::Fiber::YieldToHold();
    return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!RPC::is_enable_hook()) {
        return nanosleep_f(req, rem);
    }
    RPC::IOManager* iomanager = RPC::IOManager::GetThis();
    RPC::Fiber::ptr fiber = RPC::Fiber::GetThis();
    RPC_ASSERT2(iomanager, "iomanger is not start");
    int time_ms = req->tv_sec * 1000 + req->tv_nsec/1000 / 1000;
    iomanager->addTimer(time_ms, [iomanager, fiber]() mutable {
        iomanager->Submit(fiber);
    });
    RPC::Fiber::YieldToHold();
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (!RPC::is_enable_hook()) {
        return socket_f(domain, type, protocol);
    }
    int fd = socket_f(domain, type, protocol);
    if (fd == -1) {
        return fd;
    }
    RPC::FdMgr::GetInstance()->getFdContext(fd, true);
    return fd;
}
int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout) {
    if (!RPC::is_enable_hook()) {
        return connect_f(sockfd, addr, addrlen);
    }
    RPC::FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(sockfd);
    if (!fdctx || fdctx->isClosed()) {
        errno = EBADF;
        return -1;
    }
    if (!fdctx->isSocket()) {
        return connect_f(sockfd, addr, addrlen);
    }
    if (fdctx->userNonblock()) {
        /*  用户级非阻塞直接调用原函数， 因为原connect函数可以是非阻塞的 */
        return connect_f(sockfd, addr, addrlen);
    }

    int n = connect_f(sockfd, addr, addrlen);
    if (n != -1 || errno != EINPROGRESS) {
        return n;
    } else if (n == 0) {
        return n;
    }
    std::shared_ptr<int> timecondition(new int{0});
    RPC::IOManager* iomanager = RPC::IOManager::GetThis();
    RPC_ASSERT(iomanager);
    std::weak_ptr<int> weak_cond(timecondition);
    RPC::Timer::ptr timer;  
    if (timeout != (uint64_t)-1) {
        timer = iomanager->addConditionTimer(timeout, [iomanager, weak_cond, sockfd]() {
            auto t = weak_cond.lock();
            if (!t || *t) {
                return ;
            }
            *t = ETIMEDOUT;
            iomanager->cancelEvent(sockfd, RPC::IOManager::WRITE);
        }, weak_cond);
    }
    bool rt = iomanager->addEvent(sockfd, RPC::IOManager::Event::WRITE);
    if (!rt) {
            /* 添加事件失败*/
            RPC_LOG_ERROR(logger) << "connect_with timeout add event error";
            if (timer) {
                timer->cancel();
            }
            return -1;
    }
    RPC::Fiber::YieldToHold();
    if (timer) {
        timer->cancel();
    }
    if (*timecondition) {
        errno = *timecondition;
        return -1;
    }
    int error = 0;
    socklen_t len = sizeof(int);
    if(-1 == getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len)) {
        return -1;
    }
    if(!error) {
        return 0;
    } else {
        errno = error;
        return -1;
    }
}
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect_with_timeout(sockfd, addr, addrlen, RPC::s_connect_timeout);
}


int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int fd = do_io(sockfd, RPC::IOManager::READ, "accept", accept_f, addr, addrlen);
    // RPC_LOG_DEBUG(logger) << "accept finish, fd = " << fd;
    if (fd >= 0 && RPC::is_enable_hook()) {
        RPC::FdMgr::GetInstance()->getFdContext(fd, true);
    }
    return fd;
}

ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, RPC::IOManager::READ, "read", read_f, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, RPC::IOManager::Event::READ, "readv", readv_f, iov, iovcnt);
}


ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, RPC::IOManager::READ, "recv", recv_f, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen) {
                            return do_io(sockfd, RPC::IOManager::Event::READ, "recvfrom", recvfrom_f, buf, len, flags, src_addr, addrlen);

                        }


ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, RPC::IOManager::Event::WRITE, "write", write_f, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, RPC::IOManager::Event::WRITE, "writev", writev_f, iov, iovcnt);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return do_io(sockfd, RPC::IOManager::Event::WRITE, "send", send_f, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen) {
                        return do_io(sockfd, RPC::IOManager::Event::WRITE, "sendto", sendto_f, buf, len, flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return do_io(sockfd, RPC::IOManager::Event::WRITE, "sendmsg", sendmsg_f, msg, flags);
}

int close(int fd) {
    if (!RPC::is_enable_hook()) {
        return close_f(fd);
    }
    RPC::FdContext::ptr fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
    if (fdctx) {
        auto iom = RPC::IOManager::GetThis();
        if (iom) {
            iom->cancelAllEvent(fd);
        }
        RPC::FdMgr::GetInstance()->delFdContext(fd);
    }

    return close_f(fd);
}
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, RPC::IOManager::Event::READ, "recvmsg", recvmsg_f, msg, flags);
}
int fcntl(int fd, int cmd, ...) {
    va_list va;
    va_start(va, cmd);
    switch (cmd)
    {
    case F_SETFL:
    {

    
        int arg = va_arg(va, int);
        va_end(va);
        auto fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
        if (!fdctx || fdctx->isClosed() || !fdctx->isSocket()) {
            // 只针对socket进行hook
            return fcntl_f(fd, cmd, arg);
        }
        /* 如果设置的非阻塞就设置用户级别非阻塞*/
        fdctx->setUserNonblock(arg & O_NONBLOCK);
        /* 如果fd是系统级别非阻塞的， 参数带上O_NONBLOCK*/
        if (fdctx->sysNonblock()) {
            arg |= O_NONBLOCK;
        } else {
            arg &= ~O_NONBLOCK;
        }
        return fcntl_f(fd, cmd, arg);
       
    }
        break;
    case F_GETFL:
    {

   
        va_end(va);
        int arg = fcntl_f(fd, cmd);
        auto fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
        if (!fdctx || fdctx->isClosed() || !fdctx->isSocket()) {
            return arg;
        }
        /* 用户级别的非阻塞才是真非阻塞*/
        if (fdctx->userNonblock()) {
            arg |= O_NONBLOCK;
        } else {
            arg &= ~O_NONBLOCK;
        }
        return arg;
    }
        break;
    /* int 类型的cmd*/
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY: {

    
        int arg = va_arg(va, int);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    }
        break;
    /* void 类型cmd */
    case F_GETFD:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:{
        va_end(va);
        return fcntl_f(fd, cmd);
    }
        break;
    /* flock* 类型cmd */
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {

    
        struct flock* arg = va_arg(va, struct flock*);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    }
        break;
    /* f_owner_exlock* 类型cmd */
    case F_GETOWN_EX:
    case F_SETOWN_EX:{
        struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
        va_end(va);
        return fcntl_f(fd, cmd, arg);
    }
        break;
    default: {
        va_end(va);
        return fcntl_f(fd, cmd);
    }
        break;
    }
}


int ioctl(int fd, unsigned long request, ...) {
    va_list va;
    va_start(va, request);
    void* arg = va_arg(va, void*);
    va_end(va);
    if (request == FIONBIO) {
        bool user_nonblock = !!*(int*)arg;
        auto fdctx = RPC::FdMgr::GetInstance()->getFdContext(fd);
        if (!fdctx || fdctx->isClosed() || !fdctx->isSocket()) {
            return ioctl_f(fd, request, arg);
        }
        fdctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname,void *optval, socklen_t *optlen) {
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (!RPC::is_enable_hook()) {
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if (level == SOL_SOCKET) {
        // 只针对socket进行hook
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            // 超时时间设置
            auto fdctx = RPC::FdMgr::GetInstance()->getFdContext(sockfd);
            if (fdctx) {
                const timeval *tm = (const timeval *)optval;
                fdctx->setTimeout(optname, tm->tv_sec * 1000 + tm->tv_usec / 1000);
            }
        }
    }
    return setsockopt_f(sockfd, level, optname, optval, optlen);
}
}