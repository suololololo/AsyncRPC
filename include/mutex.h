#ifndef __MUTEX_H__
#define __MUTEX_H__
#include "noncopyable.h"
#include <pthread.h>

namespace RPC {

template <class T>
class ScopedLock {
public:
    ScopedLock(T &mutex):mutex_(mutex) {
        mutex_.lock();
    }

    ~ScopedLock() {
        mutex_.unlock();
    }
    // void lo

private:
    T &mutex_;


};


class Mutex :Noncopyable{
public:
    typedef ScopedLock<Mutex> Lock;
    Mutex() {
        pthread_mutex_init(&mutex_, nullptr);
    }

    ~Mutex() {
        pthread_mutex_destroy(&mutex_);
    }
    void lock() {
        pthread_mutex_lock(&mutex_);
    }

    void unlock() {
        pthread_mutex_unlock(&mutex_);
    }
    bool TryLock() {
        return !pthread_mutex_trylock(&mutex_);
    }


private:
    pthread_mutex_t mutex_;
};
}


#endif