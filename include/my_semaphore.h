#ifndef __MY_SEMAPHORE_H__
#define __MY_SEMAPHORE_H__
#include "noncopyable.h"
#include <semaphore.h>
#include <stdint.h>
#include <stdexcept>
namespace RPC {
class Semaphore:Noncopyable{
public:
    Semaphore(uint32_t count) {
        if (sem_init(&sem_, 0, count)) {
            throw std::logic_error("sem init error");
        }

    }
    ~Semaphore() {
        sem_destroy(&sem_);
    }

    void wait() {
        if (sem_wait(&sem_)) {
            throw std::logic_error("sem wait error");
        }
    }

    void notify() {
        if (sem_post(&sem_)) {
            throw std::logic_error("sem notify error");
        }
    }


private:
    sem_t sem_;
};


}


#endif