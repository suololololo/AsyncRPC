#include "thread.h"
#include "utils.h"
#include <exception>
namespace RPC {

static thread_local Thread * t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";
Thread::Thread(const std::string &name, std::function<void()> cb) {
    name_ = name_.empty() ? "UNKNOWN": name;
    func_ = cb;
    int res = pthread_create(&thread_, nullptr, &run, this);
    if (res) {
        throw std::logic_error("pthread_create error");
    }
    sem_.wait();
}

Thread::~Thread() {
    if (thread_) {
        pthread_detach(thread_);
    }
}

void Thread::join() {
    if (thread_) {
        int res = pthread_join(thread_, nullptr);
        if (res) {
            throw std::logic_error("pthread join error");
        }
        thread_ = 0;
    }

}

void* Thread::run(void *arg) {
    Thread* th = (Thread *)(arg);
    t_thread = th;
    t_thread_name = th->name_;
    th->id_ = GetThreadId();
    pthread_setname_np(th->thread_, th->name_.substr(0,15).c_str());
    std::function<void()> cb;
    cb.swap(th->func_);
    th->sem_.notify();
    cb();
    return nullptr;
}


Thread* Thread::GetThis() {
    return t_thread;
}

void Thread::SetName(const std::string& name) {
    if (t_thread) {
        t_thread->name_ = name;
    }
    t_thread_name = name;
}



}