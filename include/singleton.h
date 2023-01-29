#ifndef __SINGLETON_H__
#define __SINGLETON_H__
#include <memory>
namespace RPC {
template<class T>
class Singleton {
public:
    static T* GetInstance() {
        static T instance;
        return &instance;
    }
};


template<class T>
class SingletonPtr {
public:
static std::shared_ptr<T> GetInstance() {
    static std::shared_ptr<T> instance(std::make_shared<T>());
    return instance;
}
};

}


#endif