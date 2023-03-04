#ifndef __RPC_TRAITS_H__
#define __RPC_TRAITS_H__
#include <functional>
/**
 * @brief 封装RPC函数调用的类型推断相关信息
 * 
 */

namespace RPC {
/* 模板类原型 */
template <typename T>
struct function_trait;

/* 偏特化 */
template<typename ReturnType, typename ...Args>
struct function_trait<ReturnType(Args...)>
{   
    /* 函数返回值类型*/
    using return_type = ReturnType;
    /* 函数类型*/
    using function_type = ReturnType(Args...);
    /* std::function 类型*/
    using stl_function_type = std::function<ReturnType(Args...)>;
    /* 函数指针类型*/
    using function_pointer = ReturnType(*)(Args...);
    /* 参数的元组类型 */
    using arg_tuple_type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;
};

/* 函数指针*/
template<typename ReturnType, typename ...Args>
struct function_trait<ReturnType(*)(Args...)> : function_trait<ReturnType(Args...)>{};

/* std::function */
template<typename ReturnType, typename ...Args>
struct function_trait<std::function<ReturnType(Args...)>> :  function_trait<ReturnType(Args...)> {};

/* 成员函数 */
template<typename ReturnType, typename ClassType, typename...Args>
struct function_trait<ReturnType(ClassType::*)(Args...)> :  function_trait<ReturnType(Args...)> {};

template<typename ReturnType, typename ClassType, typename... Args>
struct function_trait<ReturnType(ClassType::*)(Args...) const> :  function_trait<ReturnType(Args...)> {};

template<typename ReturnType, typename ClassType, typename... Args>
struct function_trait<ReturnType(ClassType::*)(Args...) volatile> :  function_trait<ReturnType(Args...)> {};

template<typename ReturnType, typename ClassType, typename... Args>
struct function_trait<ReturnType(ClassType::*)(Args...) const volatile> :  function_trait<ReturnType(Args...)> {};


template<typename Callable>
struct function_trait : function_trait<decltype(&Callable::operator())> {}; 

}




#endif