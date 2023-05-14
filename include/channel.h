#ifndef __RPC_CHANNEL_H__
#define __RPC_CHANNEL_H__
#include "mutex.h"
#include <iostream>
namespace RPC{
template<typename T>
class ChannelImpl {
public:
    ChannelImpl(size_t capacity):isClosed_(false), capacity_(capacity){

    }

    bool push(const T &t) {
        CoMutex::Lock lock(mutex_);
        if (isClosed_) return false;
        while (msg_queue_.size() >= capacity_) {
            pushCv_.wait(lock);
            if (isClosed_) return false;
        }
        msg_queue_.push(t);
        popCv_.notify();
        return true;
    }
    bool pop(T &t) {
        CoMutex::Lock lock(mutex_);
        if (isClosed_) return false;
        while (msg_queue_.size() <= 0) {
            popCv_.wait(lock);
            if (isClosed_) return false;
        }
        t = msg_queue_.front();
        msg_queue_.pop();
        pushCv_.notify();
        return true;
    }

    /**
     * @brief 等待time_ms 时间， 读取channel数据
     * 
     * @param time_ms 
     * @param t 
     * @return true 
     * @return false 
     */
    bool waitFor(uint64_t time_ms, T &t) {
        CoMutex::Lock lock(mutex_);
        if (isClosed_) return false;
        while (msg_queue_.empty()) {
            if (!popCv_.waitFor(lock, time_ms)) return false;
            if (isClosed_) return false;
        }
        t = msg_queue_.front();
        msg_queue_.pop();
        pushCv_.notify();
        return true;
    }

    ChannelImpl& operator <<(const T &t) {
        push(t);
        return *this;
    }
    ChannelImpl& operator >>(T &t) {
        pop(t);
        return *this;
    }
    operator bool() {
        return !isClosed_;
    }
    void close() {
        CoMutex::Lock lock(mutex_);
        if (isClosed_)
            return;
        isClosed_ = true;
        pushCv_.notifyAll();
        popCv_.notifyAll();
        std::queue<T> q;
        swap(msg_queue_, q);
    }

    size_t capacity() const {
        return capacity_;
    }

    size_t size() const {
        CoMutex::Lock lock(mutex_);
        return msg_queue_.size();
    }

    bool empty() {
        return !size();
    }

private:
    bool isClosed_;
    size_t capacity_;
    CoMutex mutex_;
    /*入队条件变量*/
    CoCondVar pushCv_;
    /*出队条件变量*/
    CoCondVar popCv_;
    /*消息队列*/
    std::queue<T> msg_queue_; 
};



/* 协程通信channel*/
template<typename T>
class Channel {
public:
    Channel(size_t capacity){
        channel_impl_ = std::make_shared<ChannelImpl<T>>(capacity);
    }
    bool push(const T &t) {
        return channel_impl_->push(t);
    }
    bool pop(T &t) {
        return channel_impl_->pop(t);
    }

    bool waitFor(uint64_t time_ms, T &t) {
        return channel_impl_->waitFor(time_ms, t);
    }

    Channel& operator <<(const T &t) {
       push(t);
       return *this; 
    }

    Channel& operator >>(T &t) {
        pop(t);
        return *this;
    }

    operator bool() {
        return *channel_impl_;
    }
    void close() {
        return channel_impl_->close();
    }
    size_t capacity() const {
        return channel_impl_->capacity();
    }

    size_t size() const {
        return channel_impl_->size();
    }

    bool empty() {
        return channel_impl_->empty();
    }
    bool unique() const {
        return channel_impl_.unique();
    }
private:
    std::shared_ptr<ChannelImpl<T>> channel_impl_;

};


}



#endif