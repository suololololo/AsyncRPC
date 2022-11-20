#ifndef __FIBER_H__
#define __FIBER_H__
#include <functional>
#include <ucontext.h>
#include <memory>
namespace RPC {
class Fiber: public std::enable_shared_from_this<Fiber> {
/**
 * @brief 协程类， 提供将当前协程挂起，切出等接口
 * 
 */
public:
    typedef std::shared_ptr<Fiber> ptr;
    enum State {
        INIT,
        HOLD,
        EXEC,
        TERM,
        READY,
        EXCEPT
    };


    Fiber(std::function<void()> func, size_t stack_size);
    ~Fiber();

    /**
     * @brief 切换到当前协程，切换前要求协程为非执行状态
     * 
     */
    void Resume();

    /**
     * @brief 将当前协程挂起， 协程状态不变
     * 
     */
    void Yield();
    /**
     * @brief 重置可执行函数和重置状态
     * 
     */
    void Reset(std::function<void()> func);



public:
    static Fiber::ptr GetThis();

    static void SetThis(Fiber *fiber);

    static void EnableFiber();

    // 挂起协程, 设置协程状态为Hold
    static void YieldToHold();

    // 挂起协程，设置协程状态为Ready
    static void YieldToReady();
    // 协程执行函数
    static void MainFunc();    

private:
    Fiber();



private:
    uint64_t id_;

    uint32_t stack_size_;
    // 栈指针
    void *stack_;

    State state_;
    // 协程上下文
    ucontext_t ctx_;

    std::function<void()> func_;




};

}



#endif