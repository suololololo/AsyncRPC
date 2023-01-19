#ifndef __IO_MANAGER_H__
#define __IO_MANAGER_H__
#include "scheduler.h"
#include "mutex.h"
#include "timer.h"
namespace RPC {
/**
 * @brief 基于epoll的IO协程调度管理器(在scheduler的基础上套上了epoll)
 * 总体设计思路 epoll监听事件，将事件分发给协程调度器执行
 */
class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;
    enum Event {
        NONE = 0x0,
        READ = 0x1, //读事件
        WRITE = 0x4, //写事件
    };


    IOManager(size_t threads = 4, const std::string &name = "");
    ~IOManager();
    bool addEvent(int fd, Event event, std::function<void()> callback = nullptr);
    /**
     * @brief 直接删除事件
     */
    bool delEvent(int fd, Event event);
    /**
     * @brief 删除并触发事件
     */
    bool cancelEvent(int fd, Event event);

    static IOManager* GetThis() ;
protected:

    /**
     * @brief 对scheduler 的Notify方法重写，实现有任务时通知
     * 
     */
    void Notify() override;
    /**
     * @brief epoll wait
     * 
     */
    void Wait() override;
    /**
     * @brief 返回是否停止
     * 
     */
    bool Stopping() override;

    void onTimerInsertedAtFront() override;
    
    void contextResize(size_t size);
private:
    /**
     * @brief socket 事件的上下文
     * 
     */
    struct FdContext {
        typedef Mutex MutexType;
        struct EventContext {
            Scheduler *scheduler = nullptr; // 事件的协程调度器
            Fiber::ptr fiber; //事件的协程
            std::function<void()> callback; //事件回调函数
            bool empty() {
                return !scheduler && !fiber && !callback;
            }
        };

        EventContext& getEventContext(Event event);
        void resetContext(EventContext &event); //重置事件上下文
        void triggerEvent(Event event); // 触发事件
        int fd;              // 事件关联句柄
        Event events = NONE; // 注册的事件
        EventContext read;
        EventContext write;
        MutexType mutex;
    };  



private:
    int epollfd_;
    int pipefd_[2];

    std::vector<FdContext *> fdContext_;
    RWMutexType mutex_;
    /// 当前等待执行的IO事件数量
    std::atomic<size_t> pendingEventCount_ = {0};
};
}


#endif