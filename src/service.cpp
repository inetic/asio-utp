#include "service.hpp"

using namespace asio_utp;

asio::io_context::id service::id;

std::shared_ptr<::asio_utp::context>
service::maybe_create_context(std::shared_ptr<udp_multiplexer_impl> m)
{
    auto i = _contexts.find(m->local_endpoint());

    if (i != _contexts.end()) return i->second.lock();

    auto ctx = std::make_shared<::asio_utp::context>(std::move(m));
    _contexts[ctx->local_endpoint()] = ctx;

    return ctx;
}

