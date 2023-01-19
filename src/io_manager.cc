#include "io_manager.h"
#include "log.h"
#include "macro.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

namespace RPC {
static Logger::ptr logger = RPC_LOG_ROOT();
IOManager::IOManager(size_t threads, const std::string &name):Scheduler(threads, name) {
    int rt = pipe(pipefd_);
    RPC_ASSERT(rt == 0);
    epollfd_ = epoll_create(5);
    RPC_ASSERT(epollfd_ > 0);

    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = pipefd_[0];
    event.events = EPOLLIN | EPOLLET;
    rt = fcntl(pipefd_[0], F_SETFL, O_NONBLOCK);
    RPC_ASSERT(rt == 0);
    rt = epoll_ctl(epollfd_, EPOLL_CTL_ADD, pipefd_[0], &event);
    RPC_ASSERT(rt == 0);
    contextResize(64);
    Start();

}

IOManager::~IOManager() {
    sleep(3);
    stop_ = true;
    while(!Stopping()) {
        sleep(3);
    }
    Stop();
    close(epollfd_);
    close(pipefd_[0]);
    close(pipefd_[1]);
    for (size_t i = 0; i < fdContext_.size(); ++i) {
        if (fdContext_[i])
            delete fdContext_[i];
    }
}

void IOManager::contextResize(size_t size) {
    size_t old_size = fdContext_.size();
    fdContext_.resize(size);
    for (size_t i = old_size; i < size; ++i) {
        fdContext_[i] = new FdContext;
        fdContext_[i]->fd = i;
    }
}

bool IOManager::addEvent(int fd, Event event, std::function<void()> callback) {
    FdContext * context = nullptr;
    RWMutexType::ReadLock lock(mutex_);
    if ((int)fdContext_.size() > fd) {
        // 能匹配到socket上下文
        context = fdContext_[fd];
        lock.unlock();
    } else {
        // 不能匹配到socket上下文
        lock.unlock();
        RWMutexType::WriteLock wlock(mutex_); 
        contextResize(fd * 1.5);
        context = fdContext_[fd];
    }
    FdContext::MutexType::Lock lock1(context->mutex);
    if (context->events & event) {
        // 事件已经注册
        RPC_LOG_ERROR(logger) << "fd=" << fd << " addEvent fail, event already register. "
                << "event=" << event << " FdContext->event=" << context->events;
        RPC_ASSERT(!(context->events & event));
    }
    int op = context->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event ep_event;
    memset(&ep_event, 0, sizeof(epoll_event));
    ep_event.data.fd = fd;
    ep_event.data.ptr = context;
    Event new_event =   (Event) (event | context->events);
    context->events = new_event;
    ep_event.events = new_event | EPOLLET;
    int rt = epoll_ctl(epollfd_, op, fd, &ep_event);
    if (rt != 0) {
        RPC_LOG_ERROR(logger) << "epoll_ctl(" << epollfd_ << ", " << op << ", " << fd << ", "
                << ep_event.events << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false;
    }
    pendingEventCount_++;
    // 设置事件的回调执行相关信息（调度器，回调函数，协程）
    FdContext::EventContext &eventContext = context->getEventContext(event);
    RPC_ASSERT(eventContext.empty())
    eventContext.scheduler = Scheduler::GetThis();
    if (callback) {
        eventContext.callback.swap(callback);
    } else {
        eventContext.fiber = Fiber::GetThis();
        RPC_ASSERT(eventContext.fiber->GetState() == Fiber::EXEC);
    }

    return true;
}

bool IOManager::delEvent(int fd, Event event) {
    FdContext *context = nullptr;
    RWMutexType::ReadLock lock(mutex_);
    if ((int)fdContext_.size() <= fd) {
        RPC_LOG_ERROR(logger) << "IOManager::delEvent error fd not exist fd=" << fd << " context event=" << event;
        return false;
    }
    context = fdContext_[fd];
    lock.unlock();
    FdContext::MutexType::Lock lock1(context->mutex);
    if (!(context->events & event)) {
        RPC_LOG_ERROR(logger) << "IOManager::delEvent error event not exist fd=" << fd 
        << " context event=" << event;
        return false;
    }
    Event new_event = (Event)(context->events & ~event);
    int op = context->events? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event ep_event;
    memset(&ep_event, 0, sizeof(epoll_event));
    ep_event.data.fd = fd;
    ep_event.data.ptr = context;
    ep_event.events = new_event | EPOLLET;
    int rt = epoll_ctl(epollfd_, op, fd, &ep_event);
    if (rt != 0) {
        RPC_LOG_ERROR(logger) << "epoll_ctl(" << epollfd_ << ", " << op << ", " << fd << ", "
                << ep_event.events << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false; 
    }

    --pendingEventCount_;
    context->events = new_event;
    FdContext::EventContext &event_context = context->getEventContext(event);
    context->resetContext(event_context);
    return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
    FdContext *context = nullptr;
    RWMutexType::ReadLock lock(mutex_);
    if ((int)fdContext_.size() <= fd) {
        RPC_LOG_ERROR(logger) << "IOManager::cancelEvent error fd not exist fd=" << fd << " context event=" << event;
        return false;
    }
    context = fdContext_[fd];
    lock.unlock();
    FdContext::MutexType::Lock lock1(context->mutex);
    if (!(context->events & event)) {
        RPC_LOG_ERROR(logger) << "IOManager::cancelEvent error event not exist fd=" << fd 
            << " context event=" << event;
        return false;    
    }
    Event new_event = (Event)(context->events & ~event);
    int op = context->events ? EPOLL_CTL_MOD: EPOLL_CTL_DEL;
    epoll_event ep_event;
    memset(&ep_event, 0, sizeof(epoll_event));
    ep_event.data.ptr = context;
    ep_event.data.fd = fd;
    ep_event.events = EPOLLET | new_event;
    int rt = epoll_ctl(epollfd_, op, fd, &ep_event);
    if (rt != 0) {
        RPC_LOG_ERROR(logger) << "epoll_ctl(" << epollfd_ << ", " << op << ", " << fd << ", "
                << ep_event.events << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
        return false; 
    }
    context->triggerEvent(event);
    --pendingEventCount_;
    return true;
}

IOManager::FdContext::EventContext& IOManager::FdContext::getEventContext(Event event) {
    if (event == READ) {
        return read;
    } else if (event == WRITE) {
        return write;
    } else {
        RPC_ASSERT2(false, "getcontext");
    }
    throw std::invalid_argument("getContext invalid event");
}
void IOManager::FdContext::resetContext(EventContext &event) {
    event.scheduler = nullptr;
    event.fiber.reset();
    event.callback = nullptr;
}

void IOManager::FdContext::triggerEvent(Event event) {
    if (!(events & event)) {
        // Fd上下文没有注册该事件
        RPC_LOG_ERROR(logger) << "ASSERTION: " << (events & event)
                            << "\n" << std::to_string(event)+" & "+std::to_string(events)+" = "+std::to_string(events & event);
        assert(events &event);
    }
    events = (Event)(events & ~event);
    FdContext::EventContext &event_context = getEventContext(event);
    // 把回调函数触发一下
    if (event_context.callback != nullptr) {
        event_context.scheduler->Submit(std::move(event_context.callback));
    } else {
        event_context.scheduler->Submit(std::move(event_context.fiber));
    }
    resetContext(event_context);   
}

void IOManager::Wait() {
    const uint64_t MAX_EVENTS = 256;
    epoll_event *events = new epoll_event[MAX_EVENTS];
    std::unique_ptr<epoll_event[]> uniqueptr(events);
    while (true) {
        // 获取epoll_wait 的时间
        uint64_t next_time = getNextTimerTime();
        if (Stopping()) {
            RPC_LOG_ERROR(logger) << "already stop";
            return;
        }
        int rt = 0;
        do {
            static const int MAX_TIMEROUT = 3000;
            if (next_time != ~0ull) {
                next_time = std::min((int)next_time, MAX_TIMEROUT);
            } else {
                next_time = MAX_TIMEROUT;
            }
            rt = epoll_wait(epollfd_, events, MAX_EVENTS, (int)next_time);
            if (rt < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        } while(true);
        // 处理要触发的定时器任务
        std::vector<std::function<void()>> cb;
        listOverTimeCallback(cb);
        for (auto &it : cb) {
            Submit(it);
        }
        for (int i = 0; i < rt; ++i) {
            int fd = events[i].data.fd;
            if (fd == pipefd_[0]) {
                uint8_t dummy[256];
                while(read(pipefd_[0], dummy, sizeof(dummy)) > 0);
                continue;
            }
            FdContext *fdcontext = static_cast<FdContext *>(events->data.ptr);
            FdContext::MutexType::Lock lock(fdcontext->mutex); 
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                //关闭的连接
                events[i].events |= ((EPOLLIN | EPOLLOUT) & fdcontext->events);
            }
            int real_event = NONE;
            if (events[i].events & EPOLLIN) {
                // 可读事件
                real_event |= READ;
            }
            if (events[i].events & EPOLLOUT) {
                // 可写事件
                real_event |= WRITE;
            }
            if ((real_event & fdcontext->events) == NONE) {
                continue;
            } 
            // 每次触发的事件 都要删除（即事件触发一次）
            int left_event = fdcontext->events & ~real_event;
            int op = left_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
            events[i].events = left_event | EPOLLET;
            int rt = epoll_ctl(epollfd_, op, fdcontext->fd, &events[i]);
            if (rt != 0) {
                RPC_LOG_ERROR(logger) << "epoll_ctl(" << epollfd_ << ", " << op << ", " << fd << ", "
                    << events[i].events << "):" << rt << " (" << errno << ") (" << strerror(errno) << ")";
                continue;
            }
            // 调用事件的回调函数
            if (real_event & READ) {
                fdcontext->triggerEvent(READ);
                --pendingEventCount_;
            }
            if (real_event & WRITE) {
                fdcontext->triggerEvent(WRITE);
                --pendingEventCount_;
            }
        }
        // 挂起协程执行调度任务
        Fiber::YieldToHold();
    }
}

void IOManager::Notify() {
    if (!hasIdleThreads()) {
        return;
    }
    int rt = write(pipefd_[1], "N", 1);
    RPC_ASSERT(rt == 1);
}

bool IOManager::Stopping() {
    //定时器没有任务且 没有事件 且 协程调度器停止时 IOManager停止
    uint64_t timeout = getNextTimerTime();
    return pendingEventCount_ == 0 && Scheduler::Stopping() && timeout == ~0ull;
}

void IOManager::onTimerInsertedAtFront() {
    Notify();
}

IOManager* IOManager::GetThis() {
    static IOManager s_scheduler(4, "default scheduler name");
    IOManager *iom = dynamic_cast<IOManager *>(Scheduler::GetThis());
    return iom? iom :&s_scheduler;
}
}