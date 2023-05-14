#ifndef __RPC_ROUTE_STRATEGY_H__
#define __RPC_ROUTE_STRATEGY_H__
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
/**
 * @brief 负载均衡策略的实现
 * 
 */
namespace RPC {
enum class Strategy{
    RANDOM, //随机
    POLLING, // 轮询
    HASHIP, // 根据客户端IP地址hash
};
/**
 * @brief 路由策略接口
 * 
 */
template <class T>
class RouteStrategy {
public:
    typedef std::shared_ptr<RouteStrategy> ptr;
    virtual ~RouteStrategy(){}
    /**
     * @brief 路由选择接口
     * 
     * @param list 
     * @return T& 
     */
    virtual T& select(std::vector<T> &list) = 0;
};

namespace impl{
template <class T>
class RandomRouteStrategyImpl : public RouteStrategy<T> {
public:
    virtual T& select(std::vector<T> &list) {
        srand((unsigned)time(NULL));
        return list[rand() % list.size()];
    }
};

template <class T>
class PollingRouteStrategyImpl : public RouteStrategy<T> {
public:
    virtual T& select(std::vector<T> &list) {
        Mutex::Lock lock(mutex_);
        if (index_ >= (int) list.size()) {
            index_ = 0;
        }
        return list[index_++];
    }
private:
    Mutex mutex_;
    int index_;
};

static std::string GetLocalHost();
template <class T>
class HashIPRouteStrategyImpl : public RouteStrategy<T> {
    public:
    virtual T& select(std::vector<T> &list) {
        static std::string host = GetLocalHost();
        if (host.empty()) {
            return RandomRouteStrategyImpl<T>{}.select(list);
        }
        size_t hashcode = std::hash<std::string>()(host);
        size_t size = list.size();
        return list[hashcode % size];
    }
};

    std::string GetLocalHost() {
        int sockfd;
        struct ifconf ifconf;
        struct ifreq *ifreq = nullptr;
        char buf[512];//缓冲区
        //初始化ifconf
        ifconf.ifc_len =512;
        ifconf.ifc_buf = buf;
        if ((sockfd = socket(AF_INET,SOCK_DGRAM,0))<0)
        {
            return std::string{};
        }
        ioctl(sockfd, SIOCGIFCONF, &ifconf); //获取所有接口信息
        //接下来一个一个的获取IP地址
        ifreq = (struct ifreq*)ifconf.ifc_buf;
        for (int i=(ifconf.ifc_len/sizeof (struct ifreq)); i>0; i--)
        {
            if(ifreq->ifr_flags == AF_INET){ //for ipv4
                if (ifreq->ifr_name == std::string("eth0")) {
                    return std::string(inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr));
                }
                ifreq++;
            }
        }
        return std::string{};        
    }
}

template <class T>
class RouteEngine{
public:
    static typename RouteStrategy<T>::ptr queryStrategy(Strategy routeStrategy) {
        switch (routeStrategy) {
            case Strategy::RANDOM:
                return std::make_shared<impl::RandomRouteStrategyImpl<T>>();
            case Strategy::POLLING:
                return std::make_shared<impl::PollingRouteStrategyImpl<T>>();
            case Strategy::HASHIP:
                return std::make_shared<impl::HashIPRouteStrategyImpl<T>>();
            default:
                return std::make_shared<impl::RandomRouteStrategyImpl<T>>();
        }
    }
};



}



#endif