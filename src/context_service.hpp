#pragma once

#include <boost/asio.hpp>
#include "namespaces.hpp"

namespace asio_utp {

class context;

class context_service : public asio::io_context::service {
public:
    using endpoint_type = asio::ip::udp::endpoint;
    using socket_type = asio::ip::udp::socket;

public:
    static asio::io_context::id id;

    context_service(asio::io_context& ioc)
        : asio::io_context::service(ioc)
        , _ioc(ioc)
    {}

    std::shared_ptr<::asio_utp::context>
    get_or_create(asio::io_context& ioc, const endpoint_type& ep);
    
    void erase_context(endpoint_type ep);

private:
    asio::io_context& _ioc;
    std::map<endpoint_type, std::weak_ptr<::asio_utp::context>> _contexts;
};

} // namespace
