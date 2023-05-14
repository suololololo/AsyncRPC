#include "mutex.h"
#include "fiber.h"
#include "io_manager.h"
namespace RPC {
bool CoMutex::trylock() {
    return mutex_.trylock();
}
void CoMutex::lock() {
    if (Fiber::GetFiberId() == fiber_id_) {
        return;
    }

    while (!trylock()) {
        if (trylock()) {
            fiber_id_ = Fiber::GetFiberId();
            return;
        }
        guard_.lock();
        Fiber::ptr self = Fiber::GetThis();
        waitQueue_.push(self);
        guard_.unlock();
        Fiber::YieldToHold();
    }
    fiber_id_ = Fiber::GetFiberId();
}
void CoMutex::unlock() {
    guard_.lock();
    fiber_id_ = 0;
    Fiber::ptr fiber;
    if (!waitQueue_.empty()) {
        fiber = waitQueue_.front();
        waitQueue_.pop();
    }
    mutex_.unlock();
    guard_.unlock();
    if (fiber) {
        RPC::IOManager::GetThis()->Submit(fiber);
    }
}


void CoCondVar::notify() {
    /*从等待队列中取出一个协程来执行*/
    Fiber::ptr fiber;

    {
        MutexType::Lock lock(mutex_);
        while (waitQueue_.size()) {
            fiber = *waitQueue_.begin();
            waitQueue_.erase(waitQueue_.begin());
            if (fiber) break;
        }
        if (timer_) {
            timer_->cancel();
            timer_ = nullptr;
        }
    }
    if (fiber) {
        RPC::IOManager::GetThis()->Submit(fiber);
    }


}
void CoCondVar::notifyAll() {
    /*把等待队列中的所有协程取出来执行*/
    MutexType::Lock lock(mutex_);
    while (!waitQueue_.empty()) {
        Fiber::ptr fiber = *waitQueue_.begin();
        waitQueue_.erase(waitQueue_.begin());
        if (fiber) RPC::IOManager::GetThis()->Submit(fiber);
    }
    if (timer_) {
        timer_->cancel();
        timer_ = nullptr;
    }
}

void CoCondVar::wait(CoMutex::Lock &lock) {
    Fiber::ptr fiber = Fiber::GetThis();
    {
        MutexType::Lock lock1(mutex_);
        waitQueue_.insert(fiber);
        if (!timer_) {
            timer_ = IOManager::GetThis()->addTimer(UINT32_MAX, []{}, true);
        }
        lock.unlock();
    }
    Fiber::YieldToHold();
    lock.lock();
}

void CoCondVar::wait() {
    Fiber::ptr fiber = Fiber::GetThis();
    {
        MutexType::Lock lock(mutex_);
        waitQueue_.insert(fiber);
        if (!timer_) {
            timer_ = IOManager::GetThis()->addTimer(UINT32_MAX, []{}, true); 
        }
    }
    Fiber::YieldToHold();

}

bool CoCondVar::waitFor(CoMutex::Lock &lock, uint64_t timeout) {
    if (timeout == (uint64_t)-1) {
        wait(lock);
        return true;
    }
    Fiber::ptr fiber = Fiber::GetThis();
    IOManager* iom = IOManager::GetThis();
    std::shared_ptr<bool> timeCondtion(new bool{false});
    std::weak_ptr<bool> weakptr(timeCondtion);
    Timer::ptr timer;
    {
        MutexType::Lock lock1(mutex_);
        waitQueue_.insert(fiber);
        lock.unlock();
        timer = iom->addConditionTimer(timeout, [weakptr, iom, fiber, this]() mutable{
            MutexType::Lock lock(mutex_);
            auto it = weakptr.lock();
            if (!it) {
                return;
            }
            *it = true;
            waitQueue_.erase(fiber);
            iom->Submit(fiber);
        }, weakptr);
    }

    Fiber::YieldToHold();
    {
        MutexType::Lock lock1(mutex_);
        if (timer && !(*timeCondtion)) {
            timer->cancel();
        }
        lock.lock();
        if (*timeCondtion) {
            return false;
        }
        return true;
    }


}

CoSemaphore::CoSemaphore(uint32_t count):num_(count), used_(0) {

}
void CoSemaphore::notify() {
    CoMutex::Lock lock(mutex_);
    if (used_ > 0)
        --used_;
    var_.notify();
}
void CoSemaphore::wait() {
    CoMutex::Lock lock(mutex_);
    while (used_ >= num_) {
        var_.wait(lock);
    }
    used_++;
}


}
