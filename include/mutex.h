#ifndef __MUTEX_H__
#define __MUTEX_H__
#include "noncopyable.h"
#include <pthread.h>
/**
 * @brief 同步工具类的定义
 * 
 */
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
/**
 * @brief 读锁
 * 
 */
template<class T>
class ReadScopeLock {
public:
    ReadScopeLock(T &mutex):mutex_(mutex) {
        mutex_.rlock();
        is_lock_ = true;
    }
    ~ReadScopeLock() {
        unlock();
    }
    void lock() {
        if (!is_lock_) {
            mutex_.rlock();
            is_lock_ = true;
        }
    }

    void unlock() {
        if (is_lock_) {
            mutex_.unlock();
            is_lock_ = false;
        }
    }


private:
    T &mutex_;
    bool is_lock_;
};

template <class T>
class WriteScopeLock {
public:
    WriteScopeLock(T &mutex):mutex_(mutex) {
        mutex_.wlock();
        is_lock_ = true;
    }
    ~WriteScopeLock() {
        unlock();
    }
    void lock() {
        if (!is_lock_) {
            mutex_.wlock();
            is_lock_ = true;
        }
    }

    void unlock() {
        if (is_lock_) {
            mutex_.unlock();
            is_lock_ = false;
        }
    }
private:
    T &mutex_;
    bool is_lock_;
};

class RWMutex : Noncopyable {
public:
    typedef ReadScopeLock<RWMutex> ReadLock;
    typedef WriteScopeLock<RWMutex> WriteLock;
    RWMutex() {
        pthread_rwlock_init(&mutex, nullptr);
    }
    ~RWMutex() {
        pthread_rwlock_destroy(&mutex);
    }
    void rlock() {
        pthread_rwlock_rdlock(&mutex);
    }
    void wlock() {
        pthread_rwlock_wrlock(&mutex);
    }

    void unlock() {
        pthread_rwlock_unlock(&mutex);
    }
private:
    pthread_rwlock_t mutex;
};

}


#endif