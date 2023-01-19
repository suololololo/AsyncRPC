#include "timer.h"
#include "utils.h"
namespace RPC {
Timer::Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager *manager) 
    :ms_(ms), callback_(callback), recurring_(recurring), manager_(manager)
{
    next_ = GetCurrentMS() + ms;
}
Timer::Timer(uint64_t next):next_(next) {

}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
    if (callback_) {
        callback_ = nullptr;
        auto it = manager_->timers_.find(shared_from_this());
        manager_->timers_.erase(it);
        return true;
    }

    return false;

}
bool Timer::refresh() {
    TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
    if (!callback_) {
        return false;
    }
    auto it = manager_->timers_.find(shared_from_this());
    if (it == manager_->timers_.end()) {
        return false;
    }
    manager_->timers_.erase(it);
    next_ = GetCurrentMS() + ms_;
    manager_->timers_.insert(shared_from_this());
    return true;
}
bool Timer::reset(uint64_t ms, bool from_now) {
    if (ms_ == ms && !from_now) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
    if (!callback_) {
        return false;
    }
    auto it = manager_->timers_.find(shared_from_this());
    if (it != manager_->timers_.end()) {
        manager_->timers_.erase(it); 
    }
    uint64_t start = 0;
    if (from_now) {
        start = GetCurrentMS();
    } else {
        start = next_ - ms;
    }
    ms_ = ms;
    next_ = start + ms;
    manager_->addTimer(shared_from_this(), lock);
    return true;
}

bool Timer::Comparator::operator() (const Timer::ptr &lhs, const Timer::ptr &rhs) const {
    if (lhs->next_ < rhs->next_) {
        return true;
    }
    if (rhs->next_ < lhs->next_) {
        return false;
    }
    return lhs.get() < rhs.get();
}

TimerManager::TimerManager() {

}
TimerManager::~TimerManager() {

}
Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> callaback, bool recurring) {
    Timer::ptr timer(new Timer(ms, callaback, recurring, this));
    RWMutexType::WriteLock lock(mutex_);
    addTimer(timer, lock);
    return timer;
}
void TimerManager::addTimer(Timer::ptr timer, RWMutexType::WriteLock &lock) {
    auto it = timers_.insert(timer).first;
    bool at_front =  (it == timers_.begin());
    lock.unlock();
    if (at_front) {
        onTimerInsertedAtFront();
    }    
}
static void onTimer(std::weak_ptr<void> weak_cond, std::function<void()> callback) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        callback();
    }
}
Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> callback, std::weak_ptr<void> weak_cond, bool recurring) {
    return addTimer(ms, std::bind(&onTimer, weak_cond, callback), recurring);
}

uint64_t TimerManager::getNextTimerTime() {
    RWMutexType::ReadLock lock(mutex_);
    if (timers_.empty()) {
        return ~0ull;
    }
    const Timer::ptr &tmp = *timers_.begin();
    uint64_t curr = GetCurrentMS();
    if (curr >= tmp->next_) {
        return 0;
    } else {
        return tmp->next_ - curr;
    }
}

void TimerManager::listOverTimeCallback(std::vector<std::function<void()>> &cb) {
    uint64_t curr = GetCurrentMS();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(mutex_);
        if (timers_.empty()) {
            return;
        }
    }

    RWMutexType::WriteLock lock(mutex_);
    Timer::ptr now_timer(new Timer(curr));
    auto it = timers_.upper_bound(now_timer);
    expired.insert(expired.begin(), timers_.begin(), it);
    timers_.erase(timers_.begin(), it);
    cb.reserve(expired.size());
    for (auto &i : expired) {
        cb.push_back(i->callback_);
        if (i->recurring_) {
            i->next_ = curr + i->ms_;
            timers_.insert(i);
        } else {
            i->callback_ = nullptr;
        }
    }
}

}