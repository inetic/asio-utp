#include "context_service.hpp"
#include "context.hpp"

using namespace std;
using namespace asio_utp;

asio::io_context::id context_service::id;

std::shared_ptr<context>
context_service::get_or_create(asio::io_context& ioc, const endpoint_type& ep)
{
    auto i = _contexts.find(ep);

    if (i != _contexts.end()) return i->second.lock();

    auto ctx = make_shared<::asio_utp::context>(socket_type(ioc, ep));
    _contexts[ctx->udp_socket().local_endpoint()] = ctx;

    return ctx;
}

void context_service::erase_context(endpoint_type ep)
{
    _contexts.erase(ep);
}

