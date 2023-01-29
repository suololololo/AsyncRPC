#ifndef __FD_MANAGER_H__
#define __FD_MANAGER_H__
#include "io_manager.h"
#include "singleton.h"
#include <memory>
namespace RPC {
/**
 * @brief 文件句柄管理类
 * 
 */

class FdContext {
public:
    typedef std::shared_ptr<FdContext> ptr;
    FdContext(int fd);
    ~FdContext();
    
    bool init();
    void setTimeout(int type, uint64_t time);
    uint64_t getTimeout(int type);


    bool isClosed() const { return is_close_; }
    bool isSocket() const { return is_socket_; }
    bool userNonblock() const { return user_nonblock_; }
    bool sysNonblock() const { return sys_nonblock_; }
    void setUserNonblock(bool f) { user_nonblock_ = f; }
    void setSysNonblock(bool f) { sys_nonblock_ = f; }
private:
    int fd_;
    bool is_init_ : 1;
    bool is_close_ : 1;
    bool is_socket_ : 1;
    bool sys_nonblock_ : 1;
    bool user_nonblock_ : 1;
    uint64_t sendTimeout_;
    uint64_t recvTimeout_;
};

class FdManager{
public:
    typedef std::shared_ptr<FdManager> ptr;
    typedef RWMutex RWMutexType;
    FdManager();
    ~FdManager();
    FdContext::ptr getFdContext(int fd, bool auto_create = false);
    void delFdContext(int fd);
private:
    std::vector<FdContext::ptr> fdContextList_;
    RWMutexType mutex_;
};
typedef Singleton<FdManager> FdMgr;

}



#endif