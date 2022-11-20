#include "fiber.h"
#include "macro.h"
#include <assert.h>
#include <atomic>
#include <exception>
namespace RPC {

static std::atomic<uint64_t> s_fiber_count{0};
static std::atomic<uint64_t> s_fiber_id{0};

// 主协程指针
static thread_local Fiber::ptr t_thread_fiber = nullptr;
// 当前协程指针
static thread_local Fiber *t_fiber = nullptr;


Fiber::Fiber() {
    /**
     * @brief 主协程构造函数
     * 
     */
    state_ = EXEC;
    SetThis(this);
    if (getcontext(&ctx_) < 0) {
        RPC_ASSERT2(false, "System error: getcontext fail");
    }

    s_fiber_count++;
}


Fiber::Fiber(std::function<void()> func, size_t stack_size)
    :id_(++s_fiber_id), 
    stack_size_(stack_size),
    stack_(nullptr), 
    state_(INIT), 
    func_(func) {
    /**
     * @brief 非主协程构造函数，非主协程必须由主协程构造, 创建后不会立刻调度
     * 
     */
    RPC_ASSERT2(t_fiber, "Fiber error: no main fiber")
    RPC_ASSERT2(stack_size > 0, "System error: stack size error")
    stack_ = malloc(stack_size);
    s_fiber_count++;
    // SetThis(this);
    if (getcontext(&ctx_) < 0) {
        RPC_ASSERT2(false, "System error: getcontext fail");
    }
    // 下一context
    ctx_.uc_link = nullptr;
    //运行栈信息
    ctx_.uc_stack.ss_size = stack_size_;
    ctx_.uc_stack.ss_sp = stack_;

    makecontext(&ctx_, MainFunc, 0);
}
Fiber::~Fiber() {
    --s_fiber_count;
    if (stack_) {
        // 非主协程退出
        RPC_ASSERT(state_ == INIT || state_ == TERM || state_ == EXCEPT);
        free(stack_);
    } else {
        // 主协程退出
        RPC_ASSERT(state_==EXEC);
        RPC_ASSERT(!func_);
        if (t_fiber == this) {
            t_fiber = nullptr;
        }
    }
}



void Fiber::SetThis(Fiber *fiber) {
    t_fiber = fiber;
}

void Fiber::Resume() {
    SetThis(this);
    RPC_ASSERT2(state_ != EXEC, "fiber id =" + std::to_string(id_));
    state_ = EXEC;
    if (swapcontext(&t_thread_fiber->ctx_, &ctx_) < 0) {
        RPC_ASSERT2(false, "System error : swap fiber erro");
    }
}

/**
 * @brief 将当前协程挂起
 * 
 */
void Fiber::Yield() {
    SetThis(t_thread_fiber.get());
    if (swapcontext(&ctx_, &t_thread_fiber->ctx_) < 0) {
        RPC_ASSERT2(false, "System error: swap fiber error")
    }
}

void Fiber::Reset(std::function<void()> func) {
    RPC_ASSERT(stack_);
    RPC_ASSERT(state_ == INIT || state_ == TERM || state_ == EXCEPT);
    func_ = func;
    if (getcontext(&ctx_) < 0) {
        RPC_ASSERT2(false, "System error: getcontext fail");
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_size = stack_size_;
    ctx_.uc_stack.ss_sp = stack_;
    makecontext(&ctx_, MainFunc, 0);

}
Fiber::ptr Fiber::GetThis() {
    return t_fiber->shared_from_this();
}

// 挂起协程, 设置协程状态为Hold
void Fiber::YieldToHold() {
    Fiber::ptr curr = GetThis();
    curr->state_ = HOLD;
    curr->Yield();
}
    
// 挂起协程，设置协程状态为Ready
void Fiber::YieldToReady() {
    Fiber::ptr curr = GetThis();
    curr->state_ = READY;
    curr->Yield();
}

void Fiber::MainFunc() {
    Fiber::ptr curr = GetThis();
    RPC_ASSERT(curr);
    try {
        curr->func_();
        curr->func_ = nullptr;
        curr->state_ = TERM;
    } catch (std::exception& ex) {
        curr->state_ = EXCEPT;
        RPC_ASSERT2(false, "System error : run function error")
    } catch (...) {
        curr->state_ = EXCEPT;
        RPC_ASSERT2(false, "System error : run function error")
    }
    auto ptr = curr.get();
    curr = nullptr;
    ptr->Yield();
}


void Fiber::EnableFiber() {
    if (!t_fiber) {
        Fiber::ptr main_fiber(new Fiber);
        SetThis(main_fiber.get());
        t_thread_fiber = main_fiber;
    }
}

}
