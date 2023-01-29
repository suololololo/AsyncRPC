#ifndef __FDMANAGER_H__
#define __FDMANAGER_H__
#include "fd_manager.h"
#include "hook.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
namespace RPC {
FdContext::FdContext(int fd):fd_(fd),
    is_init_(false),
    is_close_(false),
    is_socket_(false),
    sys_nonblock_(false),
    user_nonblock_(false),
    sendTimeout_(-1),
    recvTimeout_(-1)
{
    init();
}
FdContext::~FdContext() {

}    

bool FdContext::init() {
    if (is_init_) {
        return true;
    }
    struct stat statbuf;
    int ret = fstat(fd_, &statbuf);
    if (ret == -1) {
        is_socket_ = false;
        is_init_ = false;
    } else {
        is_socket_ = S_ISSOCK(statbuf.st_mode);
        if (is_socket_) {
            int flag = fcntl_f(fd_, F_GETFL, 0);
            if (!(flag & O_NONBLOCK)) {
                fcntl_f(fd_, F_SETFL, flag | O_NONBLOCK);
            }
            sys_nonblock_ = true;
        } else {
            sys_nonblock_ = false;
        }
    }
    user_nonblock_ = false;
    is_close_ = false;
    return is_init_;
}


void FdContext::setTimeout(int type, uint64_t time) {
    if (type == SO_RCVTIMEO) {
        recvTimeout_ = time;
    } else {
        sendTimeout_ = time;
    }
}
uint64_t FdContext::getTimeout(int type) {
    if (type == SO_RCVTIMEO) {
        return recvTimeout_;
    } else {
        return sendTimeout_;
    }
}


FdManager::FdManager() {
    fdContextList_.resize(64);
}

FdManager::~FdManager() {

}

FdContext::ptr FdManager::getFdContext(int fd, bool auto_create) {
    RWMutexType::ReadLock lock(mutex_);
    if ((int)fdContextList_.size() <= fd) {
        if (!auto_create) {
            return nullptr;
        }
    } else {
        if (fdContextList_[fd] || !auto_create) {
            return fdContextList_[fd];
        }
    }
    lock.unlock();
    RWMutexType::WriteLock lock1(mutex_);
    FdContext::ptr new_fdContext(new FdContext(fd));
    fdContextList_[fd] = new_fdContext;
    return new_fdContext;
}

void FdManager::delFdContext(int fd) {
    RWMutexType::WriteLock lock(mutex_);
    if ((int)fdContextList_.size() <= fd) {
        return;
    }
    fdContextList_[fd].reset();
}

}


#endif