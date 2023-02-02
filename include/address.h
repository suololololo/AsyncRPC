#ifndef __ADDRESS_H__
#define __ADDRESS_H__
#include "utils.h"
#include <memory>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <vector>
#include <map>
/**
 * @brief 网络地址的封装
 * 
 */

namespace RPC {
/**
 * @brief 网络字节序转换主机序
 */
template<class T>
T NetworkToHost(T value) {
    return EndianCast(value);
}
/**
 * @brief 主机序转换网络字节序
 */
template<class T>
T HostToNetwork(T value) {
    return EndianCast(value);
}

class IPAddress;
/**
 * @brief 网络地址的基类
 * 
 */
class Address {
public:
    typedef std::shared_ptr<Address> ptr;
    static Address::ptr Create(const sockaddr *addr, socklen_t len);

    /**
     * @brief 根据host地址返回所有符合条件的Address
     * 
     * @param address 
     * @param host 
     * @param family 协议族(AF_INET, AF_INET6, AF_UNIX)
     * @param type sockl 类型 (sock stream, sock dgram)
     * @param protocol 协议 (IPPROTO_TCP, IPPROTO_UDP)
     * @return 返回是否转换成功
     */
    static bool Lookup(std::vector<Address::ptr> &address, const std::string &host, 
                            int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * @brief 根据host地址返回任意符合条件的Address
     * 
     * @param address 
     * @param host 
     * @param family 协议族(AF_INET, AF_INET6, AF_UNIX)
     * @param type sockl 类型 (sock stream, sock dgram)
     * @param protocol 协议 (IPPROTO_TCP, IPPROTO_UDP)
     * @return 返回任意符合条件的Address
     */
    static Address::ptr LookupAny(const std::string &host, int family = AF_INET, int type = 0, int protocol = 0);
    /**
     * @brief 根据host地址返回任意符合条件的IPAddress
     * 
     * @param address 
     * @param host 
     * @param family 协议族(AF_INET, AF_INET6, AF_UNIX)
     * @param type sockl 类型 (sock stream, sock dgram)
     * @param protocol 协议 (IPPROTO_TCP, IPPROTO_UDP)
     * @return 返回任意符合条件的Address
     */

    static std::shared_ptr<IPAddress> LookupAnyIPAdress(const std::string &host, int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * @brief 获取本机所有网卡的<网卡名，地址，子网掩码数>
     * 
     * @param result 
     * @param family family 协议族(AF_INET, AF_INET6, AF_UNIX)
     */
    static bool GetInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result, int family = AF_INET);
    
    /**
     * @brief 根据网卡名获取网卡的<地址，子网掩码数>
     * 
     * @param result 
     * @param iface 网卡名
     * @param family family 协议族(AF_INET, AF_INET6, AF_UNIX)
     */
    static bool GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>> &result, const std::string &iface, int family = AF_INET);

public:
    virtual ~Address() {}
    virtual const sockaddr * getSockAddr() const = 0;
    virtual sockaddr *getSockAddr() = 0;
    virtual socklen_t getSockAddrLen() const = 0;
    virtual std::ostream &insert(std::ostream &os) const = 0;
    std::string toString() const;
    bool operator <(const Address& rhs) const;
    bool operator ==(const Address& rhs) const;
    bool operator !=(const Address& rhs) const;
    int getFamily() const;
private:


};

class IPAddress : public Address {
public:
    typedef std::shared_ptr<IPAddress> ptr;
    /**
     * @brief 根据域名和端口号创建ip地址
     * 
     * @param host 域名
     * @param port 端口号
     * @return IPAddress::ptr 
     */
    static IPAddress::ptr Create(const char *host, uint16_t port = 0);

    /**
     * @brief 获取广播地址
     * 
     * @param prefix_len 子网掩码位数
     * @return IPAddress::ptr 
     */
    virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
    /**
     * @brief 获取该地址的网段
     * 
     * @param prefix_len 子网掩码位数
     * @return IPAddress::ptr 
     */
    virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;

    /**
     * @brief 获取子网掩码地址
     * 
     * @param prefix_len 子网掩码位数
     * @return IPAddress::ptr 
     */
    virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

    virtual uint32_t getPort() const = 0;
    virtual void setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv4Address> ptr;
    /**
     * @brief 使用点分十进制创建ipv4地址
     * 
     * @param address 点分十进制
     * @param port 
     * @return IPv4Address::ptr 
     */
    static IPv4Address::ptr Create(const char *address, uint16_t port = 0);
    IPv4Address(const sockaddr_in& address);
    /**
     * @brief 使用二进制地址构造ipv4地址
     * 
     * @param address 
     * @param port 
     */
    IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;
    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
    const sockaddr * getSockAddr() const override;
    sockaddr *getSockAddr() override;
    socklen_t getSockAddrLen() const override;
    std::ostream &insert(std::ostream &os) const override;   
private:
    sockaddr_in addr_;
};

class IPv6Address : public IPAddress {
public:
    typedef std::shared_ptr<IPv6Address> ptr;
    /**
     * @brief 使用点分十进制创建ipv6地址
     * 
     * @param address 点分十进制
     * @param port 
     * @return IPv6Address::ptr 
     */
    static IPv6Address::ptr Create(const char *address, uint32_t port = 0);
    IPv6Address();
    IPv6Address(const sockaddr_in6& address);
    /**
     * @brief 通过二进制地址构造ipv6地址
     * 
     * @param address 
     * @param port 
     */
    IPv6Address(const uint8_t address[16], uint32_t port = 0);
    IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
    IPAddress::ptr networkAddress(uint32_t prefix_len) override;
    IPAddress::ptr subnetMask(uint32_t prefix_len) override;
    uint32_t getPort() const override;
    void setPort(uint16_t v) override;
    const sockaddr * getSockAddr() const override;
    sockaddr *getSockAddr() override;
    socklen_t getSockAddrLen() const override;
    std::ostream &insert(std::ostream &os) const override;   


private:
    sockaddr_in6 addr_;
};

/**
 * @brief Unix本地套接字，用于本机网络通信，地址是由路径名标识
 * 
 */
class UnixAddress : public Address {
public:
    typedef std::shared_ptr<UnixAddress> ptr;
    UnixAddress();
    /**
     * @brief 通过路径构造UnixAddress
     * 
     * @param path 
     */
    UnixAddress(const std::string &path);
    const sockaddr * getSockAddr() const override;
    sockaddr *getSockAddr() override;
    socklen_t getSockAddrLen() const override;
    std::ostream &insert(std::ostream &os) const override;
    void setAddrLen(uint32_t v);
    std::string getPath() const;
private:
    sockaddr_un addr_;
    socklen_t len_;
};

class UnknownAddress : public Address {
public:
    typedef std::shared_ptr<UnknownAddress> ptr;
    UnknownAddress(int family);
    UnknownAddress(const sockaddr& addr);
    const sockaddr * getSockAddr() const override;
    sockaddr *getSockAddr() override;
    socklen_t getSockAddrLen() const override;
    std::ostream &insert(std::ostream &os) const override;
private:
    sockaddr addr_;
};


std::ostream& operator<<(std::ostream& os, const Address& addr);
}



#endif