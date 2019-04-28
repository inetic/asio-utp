#pragma once

#include <boost/asio.hpp>
#include "namespaces.hpp"

namespace asio_utp {

class context;

class context_service : public asio::execution_context::service {
public:
    using endpoint_type = asio::ip::udp::endpoint;
    using socket_type = asio::ip::udp::socket;

public:
    static asio::io_context::id id;

    context_service(asio::execution_context& ctx)
        : asio::execution_context::service(ctx)
    {}

    template<class Executor>
    std::shared_ptr<::asio_utp::context>
    get_or_create(Executor&, const endpoint_type&);
    
    void erase_context(endpoint_type ep);

    void shutdown() override {}

private:
    std::map<endpoint_type, std::weak_ptr<::asio_utp::context>> _contexts;
};

} // namespace

#include "context.hpp"

namespace asio_utp {

template<class Executor>
std::shared_ptr<::asio_utp::context>
context_service::get_or_create(Executor& ex, const endpoint_type& ep)
{
    auto i = _contexts.find(ep);

    if (i != _contexts.end()) return i->second.lock();

    auto ctx = std::make_shared<::asio_utp::context>(socket_type(ex, ep));
    _contexts[ctx->udp_socket().local_endpoint()] = ctx;

    return ctx;
}

} // namespace
