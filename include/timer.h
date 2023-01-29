#ifndef __TIMER_H__
#define __TIMER_H__
#include "thread.h"
#include "mutex.h"
#include <stdint.h>
#include <set>
#include <vector>
/**
 * @brief 定时器封装
 * 
 */
namespace RPC {

class TimerManager;
class Timer :public std::enable_shared_from_this<Timer>{
public:
    typedef std::shared_ptr<Timer> ptr;
    friend class TimerManager;
private:
    /**
     * @param ms 毫秒
     * @param callback 回调函数
     * @param recurring 是否循环定时器
     * @param manager 
     */
    Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager *manager);
    /**
     * @brief 仅使用绝对时间构造，该构造方法只用于比较查询
     * 
     * @param next 
     */
    Timer(uint64_t next);
    struct Comparator {
        bool operator() (const Timer::ptr &lhs, const Timer::ptr &rhs) const;
    };
public:
    /**
     * @brief 取消定时器（删除定时器）
     * 
     */
    bool cancel();
    /**
     * @brief 重新刷新定时器的时间
     * 
     */
    bool refresh();
    /**
     * @brief 定时器事件重置
     * 
     */
    bool reset(uint64_t ms, bool from_now);
private:
    uint64_t ms_; //相对时间， 相对目前多少时间间隔后执行
    uint64_t next_; // 绝对时间， 在哪个时间执行
    std::function<void()> callback_;
    bool recurring_;
    TimerManager *manager_;
};

class TimerManager {
public:
    friend class Timer;
    typedef std::shared_ptr<TimerManager> ptr;
    typedef RWMutex RWMutexType;
    TimerManager();
    virtual ~TimerManager();
    Timer::ptr addTimer(uint64_t ms, std::function<void()> callaback, bool recurring = false);
    Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> callback, std::weak_ptr<void> weak_cond, bool recurring = false);
    void addTimer(Timer::ptr timer, RWMutexType::WriteLock &lock);
    /**
     * @brief 过多少毫秒下一个定时器执行
     * 
     * @return uint64_t 
     */
    uint64_t getNextTimerTime();
    /**
     * @brief 返回已经超时的计时器的回调函数
     * 
     * @param cb 回调函数列表
     */
    void listOverTimeCallback(std::vector<std::function<void()>> &cb);
protected:
    /**
     * @brief 新建计时器在集合最前端，唤醒epoll_wait协程，重新设置epoll_Wait时间
     * 
     */
    virtual void onTimerInsertedAtFront() = 0;
private:
    std::set<Timer::ptr, Timer::Comparator> timers_;
    RWMutexType mutex_;
};

}



#endif