#include "scheduler.h"
#include "utils.h"
#include "macro.h"
#include <iostream>
namespace RPC {

static thread_local Scheduler* t_scheduler = nullptr; 


Scheduler::Scheduler(size_t threads, const std::string &name)
:threadCount_(threads), name_(name), activeThreads_(0), idleThreads_(0){
    stop_ = true;
    t_scheduler = this;

}

Scheduler::~Scheduler() {
    Stop();
    RPC_ASSERT(stop_);
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}


void Scheduler::Start() {
    MutexType::Lock lock(mutex_);
    if (stop_ == false) {
        return;
    }
    stop_ = false;
    RPC_ASSERT(threads_.empty());
    threadIds_.resize(threadCount_);
    threads_.resize(threadCount_);
    for (int i = 0; i < threadCount_; ++i) {
        threads_[i].reset(new RPC::Thread(name_+"_"+std::to_string(i), [this]{
            this->Run();
        }));
        threadIds_[i] = threads_[i]->getId();
    }
}

void Scheduler::Stop() {
    //等到没有任务的时候再停止
    while (true) {
        MutexType::Lock lock(mutex_);
        if (tasks_.empty()) {
            stop_ = true;
            break;
        }
    }
    // stop_ = true;
    for(int i = 0; i < threadCount_; ++i) {
        Notify();
    }
    std::vector<Thread::ptr> vec;
    vec.swap(threads_);
    for (auto & t : vec) {
        t->join();
    }
}

void Scheduler::Run() {
    /**
     * @brief 协程调度函数，线程冲任务队列中取出任务
     * 
     * 
     */
    SetThis();
    RPC::Fiber::EnableFiber();
    Fiber::ptr fiber;
    Fiber::ptr idle_fiber (new Fiber(std::bind(&Scheduler::Wait, this)));
    ScheduleTask task;
    while (!stop_) {
        task.Reset();
        //是否通知有任务
        bool tickle = false;
        {
            MutexType::Lock lock(mutex_);
            auto it = tasks_.begin();
            for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
                if (it->thread != -1 && it->thread != GetThreadId()) {
                    // 拿到的协程有指定线程进行执行，且不是当前线程
                    tickle = true;
                    continue;
                }
                RPC_ASSERT(*it); 
                if (it->fiber && it->fiber->GetState() == Fiber::EXEC) {
                    continue;   
                }
                task = *it;
                tasks_.erase(it);
                break;
            }
            if (it != tasks_.end()) {
                tickle = true;
            }
        }
        if (tickle) {
            Notify();
        }

        if (task.fiber && (task.fiber->GetState() != Fiber::TERM && task.fiber->GetState() != Fiber::EXCEPT)) {
            ++activeThreads_;
            // 进行协程任务调度
            task.fiber->Resume();
            --activeThreads_;
            // 调度完协程还未结束 重新加入队列
            if(task.fiber->GetState() == Fiber::READY) {
                Submit(task.fiber);
            }
            task.Reset();
        } else if (task.func) {
            if (fiber) {
                fiber->Reset(task.func);
            } else {
                fiber.reset(new Fiber(task.func));
            }
            task.Reset();
            //调度协程
            ++activeThreads_;
            fiber->Resume();
            --activeThreads_;
            if (fiber->GetState() == Fiber::READY) {
                Submit(fiber);
                fiber.reset();
            } else if (fiber->isTerminate()) {
                fiber->Reset(nullptr);
            } else {
                // 释放协程
                fiber = nullptr;
            }

        } else {
            if (idle_fiber->GetState() == Fiber::TERM) {
                break;
            }
            ++idleThreads_;
            idle_fiber->Resume();
            --idleThreads_;
        }
    }
}


Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

void Scheduler::SetThis() {
    t_scheduler = this;
}

bool Scheduler::Stopping() {
    MutexType::Lock lock(mutex_);
    return stop_ && tasks_.empty() && activeThreads_ == 0;
}

    /**
     * @brief 调度器无任务调度时执行wait
     * 
     */

void Scheduler::Wait() {

    std::cout << "idle" << std::endl;
    while (!Stopping()) {
        RPC::Fiber::YieldToHold();
    }
    std::cout << "idle fiber exit" << std::endl;
}

void Scheduler::Notify() {
    std::cout << "notify" << std::endl;
}


}
