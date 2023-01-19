#include "address.h"
#include "utils.h"
#include "log.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
namespace RPC {
static Logger::ptr logger = RPC_LOG_ROOT();

template<class T>
static T CreateMask(uint32_t bit) {
    // 0000000011111
    return (1 << (sizeof(T) * 8 - bit)) -1;
}

template<class T>
static uint32_t CountBytes(T value) {
    uint32_t result = 0;
    for(; value; ++result) {
        value &= value - 1;
    }
    return result;
}
Address::ptr Address::Create(const sockaddr *addr, socklen_t len) {
    if (addr == nullptr) {
        return nullptr;
    }
    Address::ptr res;
    switch (addr->sa_family) {
        case AF_INET:
            res.reset(new IPv4Address(*(const sockaddr_in *)addr));
            break;
        case AF_INET6:
            res.reset(new IPv6Address(*(const sockaddr_in6 *)addr));
            break;
        default:
            res.reset(new UnknownAddress(*addr));
            break;
    }
    return res;
}
bool Address::Lookup(std::vector<Address::ptr> &address, const std::string &host, int family, int type, int protocol) {
    addrinfo hints, *results, *next;
    hints.ai_family = family;
    hints.ai_protocol = protocol;
    hints.ai_socktype = type;
    hints.ai_flags = 0;
    hints.ai_addrlen = 0;
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;

    std::string node;
    const char *service = NULL;
    if (!host.empty() && host[0]=='[') {
        const char *endipv6 = (const char *)memchr(host.c_str()+1, ']', host.size() - 1);
        if (endipv6) {
            if (*(endipv6+1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }
    if (node.empty()) {
        service = (const char *)memchr(host.c_str(), ':', host.size());
        if (service) {
            if (!memchr(service+1, ':', host.c_str()+ host.size() - service -1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if (node.empty()) {
        node = host;
    }
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if (error != 0) {
        RPC_LOG_ERROR(logger) << "Address::Lookup getaddress(" << host << ", "
            << family << ", " << type << ") err=" << error << " errstr="
            << gai_strerror(error);
        return false;
    }
    next = results;
    while (next) {
        address.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
        next = next->ai_next;
    }
    freeaddrinfo(results);
    return true;
}


Address::ptr Address::LookupAny(const std::string &host, int family, int type, int protocol) {
    std::vector<Address::ptr> res;
    if (Lookup(res, host, family, type, protocol)) {
        return res[0];
    }
    return nullptr;
}
std::shared_ptr<IPAddress> Address::LookupAnyIPAdress(const std::string &host, int family, int type, int protocol) {
    std::vector<Address::ptr> res;
    if (Lookup(res, host, family, type, protocol)) {
        for (auto &i : res) {
            IPAddress::ptr addr = std::dynamic_pointer_cast<IPAddress>(i);
            if (addr) {
                return addr;
            }
        }
    }
    return nullptr;
}
bool Address::GetInterfaceAddress(std::multimap<std::string, std::pair<Address::ptr, uint32_t>> &result, int family) {
    struct ifaddrs *next, *results;
    int error = getifaddrs(&results);
    if (error != 0) {
        RPC_LOG_ERROR(logger) << "Address::GetInterfaceAddress getifaddrs" << "err=" << error << " errstr="
            << gai_strerror(error);
        return false;
    }
    next = results;
    try {
        while (next) {
            Address::ptr address;
            uint32_t prefix_len = ~0u;
            std::string interface_name = next->ifa_name;
            // 协议类型不符合 
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                next = next->ifa_next;
                continue;
            }
            if (next->ifa_addr->sa_family == AF_INET) {
                address = Create(next->ifa_addr, sizeof(sockaddr_in));
                uint32_t netmask = ((sockaddr_in *)next->ifa_addr)->sin_addr.s_addr;
                prefix_len = CountBytes(netmask);
            } else if (next->ifa_addr->sa_family == AF_INET6) {
                address = Create(next->ifa_addr, sizeof(sockaddr_in6));
                in6_addr& netmask = ((sockaddr_in6 *)next->ifa_addr)->sin6_addr;
                prefix_len = 0;
                for(int i = 0; i < 16; ++i) {
                    prefix_len += CountBytes(netmask.s6_addr[i]);
                }
            } 
            if(address) {
                result.insert(std::make_pair(next->ifa_name,
                                std::make_pair(address, prefix_len)));
            }
            next = next->ifa_next;
        }
    } catch(...) {
        RPC_LOG_ERROR(logger) << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return !result.empty();

}
bool Address::GetInterfaceAddress(std::vector<std::pair<Address::ptr, uint32_t>> &result, const std::string &iface, int family ) {
    if (iface.empty() || iface == "*") {
        if (family == AF_INET || family == AF_UNSPEC) {
            result.push_back(std::make_pair<Address::ptr, uint32_t>(Address::ptr(new IPv4Address()), 0u));
        }
        if (family == AF_INET6 || family == AF_UNSPEC) {
            result.push_back(std::make_pair<Address::ptr, uint32_t>(Address::ptr(new IPv6Address()), 0u));
        }
        return true;
    }
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>> m;
    if (!GetInterfaceAddress(m, family)) {
        return false;
    }
    auto its = m.equal_range(iface);
    for (; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();
}

std::string Address::toString() const {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}
bool Address::operator <(const Address& rhs) const{
    socklen_t minlen = std::min(getSockAddrLen(), rhs.getSockAddrLen());
    int result = memcmp(getSockAddr(), rhs.getSockAddr(), minlen);
    if(result < 0) {
        return true;
    } else if(result > 0) {
        return false;
    } else if(getSockAddrLen() < rhs.getSockAddrLen()) {
        return true;
    }
    return false;
}
bool Address::operator ==(const Address& rhs) const{
    return getSockAddrLen() == rhs.getSockAddrLen() && memcmp(getSockAddr(), rhs.getSockAddr(), getSockAddrLen())==0;
}
bool Address::operator !=(const Address& rhs) const{
    return !(*this == rhs);
}
int Address::getFamily() const {
    return getSockAddr()->sa_family;
}


IPAddress::ptr IPAddress::Create(const char *host, uint16_t port) {
    addrinfo hint, *result;
    memset(&hint, 0, sizeof(addrinfo));
    hint.ai_family = AF_UNSPEC;
    int error = getaddrinfo(host, NULL, &hint, &result);
    if (error != 0) {
        RPC_LOG_ERROR(logger) << "IPAddress::Create(" << host
                                  << ", " << port << ") error=" << error
                                  << " errstr=" << gai_strerror(error);
        return nullptr;
    }
    try {
        Address::ptr address = Address::Create(result->ai_addr, (socklen_t)result->ai_addrlen);
        IPAddress::ptr res = std::dynamic_pointer_cast<IPAddress>(address);
        if (res) {
            res->setPort(port);
        }
        freeaddrinfo(result);
        return res;
    } catch (...) {
        freeaddrinfo(result);    
        return nullptr;
    }
}

IPv4Address::ptr IPv4Address::Create(const char *address, uint16_t port) {
    IPv4Address::ptr addr(new IPv4Address);
    addr->addr_.sin_port = HostToNetwork(port);
    int result = inet_pton(AF_INET, address, &addr->addr_.sin_addr);
    if (result <= 0) {
        RPC_LOG_ERROR(logger) << "IPv4Address::Create(" << address << ", "
                << port << ") rt=" << result << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return addr;
}
IPv4Address::IPv4Address(const sockaddr_in& address):addr_(address) {

}
IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_port = HostToNetwork(port);
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = HostToNetwork(address);
}
IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in baddr(addr_);
    baddr.sin_addr.s_addr |= EndianCast(CreateMask<uint32_t>(prefix_len));

    return IPv4Address::ptr(new IPv4Address(baddr));

}
IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in naddr(addr_);
    naddr.sin_addr.s_addr &= EndianCast(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(naddr));
}
IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
    if (prefix_len > 32) {
        return nullptr;
    }
    sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr |= ~EndianCast(CreateMask<uint32_t>(prefix_len));
    return IPv4Address::ptr(new IPv4Address(saddr));
}
uint32_t IPv4Address::getPort() const  {
    return NetworkToHost(addr_.sin_port);
};
void IPv4Address::setPort(uint16_t v) {
    addr_.sin_port = HostToNetwork(v);
}
const sockaddr * IPv4Address::getSockAddr() const {
    return (const sockaddr *)&addr_;
};

sockaddr *IPv4Address::getSockAddr() {
    return (sockaddr *)&addr_; 
}
socklen_t IPv4Address::getSockAddrLen() const {
    return sizeof(addr_);
}
std::ostream &IPv4Address::insert(std::ostream &os) const {
    uint32_t addr = NetworkToHost(addr_.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    os << ":" << NetworkToHost(addr_.sin_port);
    return os;
} 

IPv6Address::ptr IPv6Address::Create(const char *address, uint32_t port) {
    IPv6Address::ptr addr(new IPv6Address);
    addr->addr_.sin6_port = HostToNetwork(port);
    int res = inet_pton(AF_INET6, address, &addr->addr_.sin6_addr);
    if (res <= 0) {
        RPC_LOG_ERROR(logger) << "IPv6Address::Create(" << address << ", "
                << port << ") rt=" << res << " errno=" << errno
                << " errstr=" << strerror(errno);
        return nullptr;
    }
    return addr;
}

IPv6Address::IPv6Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address):addr_(address) {

}

IPv6Address::IPv6Address(const uint8_t address[16], uint32_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = HostToNetwork(port);
    memcpy(&addr_.sin6_addr.s6_addr, address, 16);
}
IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
    sockaddr_in6 baddr(addr_);
    baddr.sin6_addr.s6_addr[prefix_len / 8] |=
        CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(baddr));
}
IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len){
    sockaddr_in6 naddr(addr_);
    naddr.sin6_addr.s6_addr[prefix_len / 8] &=
        CreateMask<uint8_t>(prefix_len % 8);
    for(int i = prefix_len / 8 + 1; i < 16; ++i) {
        naddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return IPv6Address::ptr(new IPv6Address(naddr));
}
IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len){
    sockaddr_in6 subnet;
    memset(&subnet, 0, sizeof(subnet));
    subnet.sin6_family = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len /8] =
        ~CreateMask<uint8_t>(prefix_len % 8);

    for(uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return IPv6Address::ptr(new IPv6Address(subnet));
}
uint32_t IPv6Address::getPort() const {
    return NetworkToHost(addr_.sin6_port);
}
void IPv6Address::setPort(uint16_t v) {
    addr_.sin6_port = NetworkToHost(v);
}
const sockaddr * IPv6Address::getSockAddr() const {
    return (const sockaddr *)&addr_;
}
sockaddr *IPv6Address::getSockAddr() {
        return (sockaddr *)&addr_;
}
socklen_t IPv6Address::getSockAddrLen() const {
    return sizeof(addr_);
}
std::ostream &IPv6Address::insert(std::ostream &os) const {
    os << "[";
    uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
    bool used_zeros = false;
    for(size_t i = 0; i < 8; ++i) {
        if(addr[i] == 0 && !used_zeros) {
            continue;
        }
        if(i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        if(i) {
            os << ":";
        }
        os << std::hex << (int)NetworkToHost(addr[i]) << std::dec;
    }

    if(!used_zeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << NetworkToHost(addr_.sin6_port);
    return os;
}
static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;
UnixAddress::UnixAddress() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    len_ = offsetof(sockaddr_un, sun_path)+ MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string &path) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX; 
    len_ = path.size() + 1;
    if(!path.empty() && path[0] == '\0') {
        --len_;
    }
    if(len_ > sizeof(addr_.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(addr_.sun_path, path.c_str(), len_);
    len_ += offsetof(sockaddr_un, sun_path);
}
const sockaddr * UnixAddress::getSockAddr() const {
    return (const sockaddr *)&addr_;
}
sockaddr *UnixAddress::getSockAddr() {
    return (sockaddr *)&addr_;
}
socklen_t UnixAddress::getSockAddrLen() const {
    return len_;
}
std::ostream &UnixAddress::insert(std::ostream &os) const {
    if(len_ > offsetof(sockaddr_un, sun_path)
            && addr_.sun_path[0] == '\0') {
        return os << "\\0" << std::string(addr_.sun_path + 1,
                len_ - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << addr_.sun_path;
}
void UnixAddress::setAddrLen(uint32_t v) {
    len_ = v;
}
std::string UnixAddress::getPath() const {
    std::stringstream ss;
    if(len_ > offsetof(sockaddr_un, sun_path)
            && addr_.sun_path[0] == '\0') {
        ss << "\\0" << std::string(addr_.sun_path + 1,
                len_ - offsetof(sockaddr_un, sun_path) - 1);
    } else {
        ss << addr_.sun_path;
    }
    return ss.str();
}

UnknownAddress::UnknownAddress(int family) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sa_family = family;
}
UnknownAddress::UnknownAddress(const sockaddr& addr):addr_(addr) {

}
const sockaddr * UnknownAddress::getSockAddr() const {
    return &addr_;
}
sockaddr *UnknownAddress::getSockAddr()  {
    return &addr_;
}
socklen_t UnknownAddress::getSockAddrLen() const {
    return sizeof(addr_);
}
std::ostream &UnknownAddress::insert(std::ostream &os) const {
    os << "[UnknownAddress family=" << addr_.sa_family << "]";
    return os;
}
std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.insert(os);
}

}