#ifndef __SOCKET_H__
#define __SOCKET_H__
#include "address.h"
namespace RPC {
class Socket : public std::enable_shared_from_this<Socket>{
public:
    typedef std::shared_ptr<Socket> ptr;
    /**
     * @brief socket 类型
     * 
     */
    enum Type {
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };
    /**
     * @brief 协议族类型
     * 
     */
    enum FAMILY {
        IPV4 = AF_INET,
        IPV6 = AF_INET6,
        UNIX = AF_UNIX
    };

    static Socket::ptr CreateTCP(Address::ptr address);
    static Socket::ptr CreateUDP(Address::ptr address);
    /**
     * @brief 创建 AF_INET + SOCK_STREAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateTCPSocket();
    /**
     * @brief 创建 AF_INET + SOCK_DGRAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateUDPSocket();
    /**
     * @brief 创建 AF_INET6 + SOCK_STREAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateTCPSocket6();
    /**
     * @brief 创建 AF_INET6 + SOCK_DGRAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateUDPSocket6();
    /**
     * @brief 创建 AF_UNIX + SOCK_STREAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateUnixTCPSocket();
    /**
     * @brief 创建 AF_UNIX + SOCK_DGRAM 的socket
     * 
     * @return Socket::ptr 
     */
    static Socket::ptr CreateUnixUDPSocket();    
    Socket(int family, int type, int protocol);

    virtual ~Socket();
    /**
     * @brief Set the Option object
     * 
     * @param level (SOL_SOCKET:通用套接字选项, IPPROTO_IP:IP选项, IPPROTO_TCP:TCP选项)
     * @param optname 
     * @param result 
     * @param len 
     */
    bool setOption(int level, int optname, void *result, size_t len);
    bool getOption(int level, int optname, void *result, size_t* len);
    template <class T>
    bool setOption(int level, int optname, T *result) {
        return setOption(level, optname, result, sizeof(T));
    }
    template <class T>
    bool getOption(int level, int optname, T *result) {
        size_t len = sizeof(T);
        return getOption(level, optname, result, &len);
    }

    void setSendTimeout(uint64_t time);
    uint64_t getSendTimeout() const;

    bool bind(const RPC::Address::ptr address);
    bool connect(const RPC::Address::ptr address, uint64_t timeout_ms = -1);
    /**
     * @brief 
     * 
     * @param backlog 挂起的连接队列的最大长度
     */
    bool listen(int backlog = SOMAXCONN);
    bool close();
    Socket::ptr accept();
    int recv(void *buff, size_t length, int flag);
    int recv(iovec *buff, size_t length, int flag);
    int send(const void *buff, size_t length, int flag);
    int send(const iovec *buff, size_t length, int flag);
    /**
     * @brief UDP 接收信息
     */
    int recvfrom(void *buf, size_t len, Address::ptr source, int flags);
    /**
     * @brief UDP 发送信息
     */
    int sendto(const void *buf, size_t len, const Address::ptr dest, int flags);
    /**
     * @brief 获取本地地址
     * 
     */
    int recvfrom(iovec *buf, size_t len, Address::ptr source, int flags);
    int sendto(const iovec *buf, size_t len, const Address::ptr dest, int flags);

    Address::ptr getLocalAddress();
    /**
     * @brief 获取远程地址
     * 
     */
    Address::ptr getRemoteAddress();
    /**
     * @brief Socket对象是否有效，即句柄是否正确
     * 
     */
    bool isVaild() const;

    int getError();
    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();
    /**
     * @brief 输出socket相关信息
     * 
     */
    std::ostream & dump(std::ostream& os) const;
private:
    /**
     * @brief 创建一个socket句柄
     * 
     */
    void newSocket();
    /**
     * @brief 初始化socket设置，设置SO_REUSEADDR
     * 
     */
    void initSocket();
    /**
     * @brief 使用已经存在的文件句柄初始化socket对象
     * 
     */
    bool initSocketByFd(int fd);
private:
    int fd_;
    int family_;
    int type_;
    int protocol_;
    bool is_connected_;
    Address::ptr local_address_;
    Address::ptr remote_address_;
}; 
}


#endif