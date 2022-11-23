#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__
#include "fiber.h"
#include "thread.h"
#include "mutex.h"
#include <memory>
#include <list>
#include <vector>
#include <atomic>
namespace RPC {
class Scheduler {
/**
 * @brief 协程调度器 M:N的调度模型
 *         调度器中含有M的线程， 每个线程对应着一个主协程，每个主协程调度N的协程
 * 
 */
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef Mutex MutexType;
    Scheduler(size_t threads = 4, const std::string &name = "");
    virtual ~Scheduler();
    /**
     * @brief 启动调度器
     * 
     */
    void Start();
    void Stop();

    template<class FiberOrCb>
    Scheduler* Submit(FiberOrCb& fc, int thread = -1) {
        bool need_notify = false;
        {
            MutexType::Lock lock(mutex_);
            need_notify = SubmitNoLock(fc, thread);
        }
        if (need_notify) {
            Notify();
        }
        return this;
    }

    static Scheduler* GetThis();
protected:
    /**
     * @brief 线程执行的调度函数
     * 
     */
    void Run();

    /**
     * @brief 通知协程调度器有任务
     * 
     */
    virtual void Notify();
    /**
     * @brief 设置当前协程调度器
     * 
     */
    void SetThis();

    /**
     * @brief 返回是否能停止
     * 
     * @return true 
     * @return false 
     */
    virtual bool Stopping();

    /**
     * @brief 调度器无任务调度时执行wait
     * 
     */

    virtual void Wait();

private:
    /**
     * @brief 调度任务结构体
     * 
     */
    struct ScheduleTask{
        Fiber::ptr fiber;
        std::function<void()> func;
        int thread;
        ScheduleTask() {
            thread = -1;
            fiber = nullptr;
            func = nullptr;
        }
        ScheduleTask(Fiber::ptr &f, int t = -1) {
            fiber = f;
            thread = t;
            func = nullptr;
        }
        ScheduleTask(std::function<void()> &fc, int t = -1) {
            func = fc;
            thread = t;
            fiber = nullptr;
        }
        ScheduleTask(Fiber::ptr &&f, int t = -1) {
            fiber = std::move(f);
            thread = t;
            func = nullptr;
        }
        ScheduleTask(std::function<void()> &&fc, int t = -1) {
            func = std::move(fc);
            thread = t;
            fiber = nullptr;
        }
        void Reset() {
            fiber = nullptr;
            func = nullptr;
            thread = -1;
        }
        operator bool() {
            return fiber || func;
        }
    };

    template<class FiberOrcb>
    bool SubmitNoLock(FiberOrcb &&fc, int thread) {
        bool need_notify = tasks_.empty();
        ScheduleTask task(std::forward<FiberOrcb>(fc), thread);
        if (task) {
            tasks_.push_back(task);
        }
        return need_notify;
    }

protected:
    std::vector<int> threadIds_;
    size_t threadCount_;
    // 活跃线程数
    std::atomic<size_t> activeThreads_;
    // 空闲线程数
    std::atomic<size_t> idleThreads_;
private:
    std::list<ScheduleTask> tasks_;
    std::vector<Thread::ptr> threads_;
    std::string name_;
    bool stop_;
    MutexType mutex_;


};

}



#endif