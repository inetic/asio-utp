#pragma once

#include <asio_utp/socket.hpp>

namespace asio_utp {

// A class implementing the Protocol and AcceptableProtocol concepts
// TODO: It currently implements only a subset of the required API
class protocol {
public:
    using endpoint = boost::asio::ip::udp::endpoint;
    using socket = ::asio_utp::socket;
    using resolver = boost::asio::ip::udp::resolver;
};

} // namespace
