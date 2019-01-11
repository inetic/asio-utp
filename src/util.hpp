#pragma once

#include <boost/asio/ip/udp.hpp>
#include "namespaces.hpp"

namespace asio_utp { namespace util {

inline
sockaddr to_sockaddr(const asio::ip::udp::endpoint& ep)
{
    // TODO: IPv6
    assert(ep.address().is_v4());

    sockaddr_in src_addr;

    src_addr.sin_family = AF_INET;
    src_addr.sin_port   = htons(ep.port());
    src_addr.sin_addr   = *((in_addr*) ep.address().to_v4().to_bytes().data());

    return *((sockaddr*)&src_addr);
}

inline
asio::ip::udp::endpoint to_endpoint(const sockaddr& addr)
{
    assert(addr.sa_family == AF_INET);

    asio::ip::udp::endpoint ret;

    sockaddr_in* addr_in = (sockaddr_in*) &addr;

    ret.port(ntohs(addr_in->sin_port));

    unsigned char* addr_from = (unsigned char*) &addr_in->sin_addr;
    asio::ip::address_v4::bytes_type addr_to;

    for (auto& a : addr_to) {
        a = *(addr_from++);
    }

    ret.address(asio::ip::address_v4(addr_to));

    return ret;
}

}} // namespaces
