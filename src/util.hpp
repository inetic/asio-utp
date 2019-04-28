#pragma once

#include <boost/asio/ip/udp.hpp>
#include "namespaces.hpp"

namespace asio_utp { namespace util {

inline
sockaddr_in to_sockaddr_v4(const asio::ip::udp::endpoint& ep)
{
    sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(ep.port());
    addr.sin_addr   = *((in_addr*) ep.address().to_v4().to_bytes().data());

    return addr;
}

inline
sockaddr_in6 to_sockaddr_v6(const asio::ip::udp::endpoint& ep)
{
    assert(ep.address().is_v6());

    sockaddr_in6 addr;

    addr.sin6_family   = AF_INET6;
    addr.sin6_port     = htons(ep.port());
    addr.sin6_flowinfo = 0;
    addr.sin6_scope_id = htons(ep.address().to_v6().scope_id());

    auto bs = ep.address().to_v6().to_bytes();

    for (size_t i = 0; i < 16; ++i) {
        addr.sin6_addr.s6_addr[i] = bs[i];
    }

    return addr;
}

inline
sockaddr_storage
to_sockaddr(const asio::ip::udp::endpoint& ep)
{
    sockaddr_storage d;

    if (ep.address().is_v4()) {
        sockaddr_in addr = to_sockaddr_v4(ep);
        memcpy(&d, &addr, sizeof(addr));
    }
    else {
        assert(ep.address().is_v6());
        sockaddr_in6 addr = to_sockaddr_v6(ep);
        memcpy(&d, &addr, sizeof(addr));
    }

    return d;
}

inline
size_t sockaddr_size(const sockaddr_storage& addr)
{
    switch (addr.ss_family) {
        case AF_INET: return sizeof(sockaddr_in);
        case AF_INET6: return sizeof(sockaddr_in6);
        default: assert(0); return 0;
    }
}

inline
asio::ip::udp::endpoint to_endpoint_v4(const sockaddr_in& addr)
{
    asio::ip::udp::endpoint ret;

    ret.port(ntohs(addr.sin_port));

    unsigned char* addr_from = (unsigned char*) &addr.sin_addr;
    asio::ip::address_v4::bytes_type addr_to;

    for (auto& a : addr_to) {
        a = *(addr_from++);
    }

    ret.address(asio::ip::address_v4(addr_to));

    return ret;
}

inline
asio::ip::udp::endpoint to_endpoint_v6(const sockaddr_in6& addr)
{
    asio::ip::udp::endpoint ret;

    ret.port(ntohs(addr.sin6_port));

    unsigned char* addr_from = (unsigned char*) &addr.sin6_addr.s6_addr;
    asio::ip::address_v6::bytes_type addr_to;

    for (auto& a : addr_to) {
        a = *(addr_from++);
    }

    ret.address(asio::ip::address_v6(addr_to));

    return ret;
}

inline
asio::ip::udp::endpoint to_endpoint(const sockaddr& addr)
{
    if (addr.sa_family == AF_INET) {
        return to_endpoint_v4(reinterpret_cast<const sockaddr_in&>(addr));
    }
    else {
        assert(addr.sa_family == AF_INET6);
        return to_endpoint_v6(reinterpret_cast<const sockaddr_in6&>(addr));
    }
}

inline
asio::ip::udp::endpoint to_endpoint(const sockaddr_storage& addr)
{
    if (addr.ss_family == AF_INET) {
        return to_endpoint_v4(reinterpret_cast<const sockaddr_in&>(addr));
    }
    else {
        assert(addr.ss_family == AF_INET6);
        return to_endpoint_v6(reinterpret_cast<const sockaddr_in6&>(addr));
    }
}

}} // namespaces
