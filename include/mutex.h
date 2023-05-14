#ifndef __MUTEX_H__
#define __MUTEX_H__
#include "noncopyable.h"
#include <pthread.h>
#include <queue>
#include <memory>
#include <set>
/**
 * @brief 同步工具类的定义
 * 
 */
namespace RPC {
class Fiber;
class Timer;
template <class T>
class ScopedLock {
public:
    ScopedLock(T &mutex):mutex_(mutex) {
        mutex_.lock();
        lock_ = true;
    }

    ~ScopedLock() {
        mutex_.unlock();
    }
    void lock() {
        if (!lock_) {
            mutex_.lock();
            lock_ = true;
        }
    }

    void unlock() {
        if (lock_) {
            mutex_.unlock();
            lock_ = true;
        }
    }
    bool trylock() {
        if (!lock_) {
            lock_ = mutex_.trylock();
        }
        return lock_;
    }

private:
    T &mutex_;
    bool lock_;

};


class Mutex : public Noncopyable{
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
    bool trylock() {
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

class RWMutex : public Noncopyable {
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

class SpinLock : public Noncopyable {
public:
    typedef ScopedLock<SpinLock> Lock;
    SpinLock() {
        pthread_spin_init(&mutex_, 0);
    }
    ~SpinLock() {
        pthread_spin_destroy(&mutex_);
    }
    void lock() {
        pthread_spin_lock(&mutex_);
    }
    bool trylock() {
        return !pthread_spin_trylock(&mutex_);
    }
    void unlock() {
        pthread_spin_unlock(&mutex_);
    }

private:
    pthread_spinlock_t mutex_;
};


/**
 * @brief 基于自旋锁的协程锁
 * 
 */
class CoMutex : public Noncopyable {
public:
    typedef ScopedLock<CoMutex> Lock;
    
    bool trylock();
    void lock();
    void unlock();

private:
    SpinLock mutex_; /*协程持有的锁*/
    SpinLock guard_; /*等待队列的锁*/
    uint64_t fiber_id_; /*持有锁的协程id*/
    std::queue<std::shared_ptr<Fiber>> waitQueue_;
};


/**
 * @brief 基于自旋锁的协程条件变量
 * 
 */
class CoCondVar : public Noncopyable {
public:
    typedef SpinLock MutexType;
    void notify();
    void notifyAll();
    /**
     * @brief 加锁等待唤醒
     * 
     * @param lock 
     */
    void wait(CoMutex::Lock &lock);
    /**
     * @brief 不加锁等待唤醒
     * 
     */
    void wait(); 
    bool waitFor(CoMutex::Lock &lock, uint64_t timeout);

private:
    MutexType mutex_;
    std::set<std::shared_ptr<Fiber>> waitQueue_;
    /* 空定时器，让调度器保持调度不退出*/
    std::shared_ptr<Timer> timer_; 
};

class CoSemaphore : public Noncopyable {
public:
    CoSemaphore(uint32_t count);
    void notify();
    void wait();
private:
    uint32_t num_; /*信号总量*/
    uint32_t used_; /*已使用信号量*/
    CoMutex mutex_;
    CoCondVar var_;
};


}


#endif