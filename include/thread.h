#ifndef __THREAD_H__
#define __THREAD_H__
#include "my_semaphore.h"
#include <memory>
#include <functional>
namespace RPC {
class Thread {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(const std::string &name, std::function<void()> cb);
    ~Thread();
    void join();
    const pid_t getId() const { return id_;}
    static const std::string& getName();

    static void* run(void *arg);
    static Thread *GetThis();
    static void SetName(const std::string& name);

private:
    Thread(const Thread& thread) = delete;
    Thread(const Thread&& thread) = delete;
    Thread& operator=(const Thread &thread) = delete;

private:
    pid_t id_;
    pthread_t thread_;
    std::string name_;
    std::function<void()> func_;
    Semaphore sem_{1};
};
}




#endif