#include <utp/socket.hpp>
#include "namespaces.hpp"

using namespace std;
using namespace utp;

socket::socket(boost::asio::io_service& ios, const endpoint_type& ep)
    : _ios(&ios)
    , _socket_impl(make_shared<socket_impl>(ios))
{
    _socket_impl->bind(ep);
}

boost::asio::ip::udp::endpoint socket::local_endpoint() const
{
    assert(_socket_impl); // TODO: throw
    return _socket_impl->local_endpoint();
}

bool socket::is_open() const {
    return _socket_impl && _socket_impl->is_open();
}

void socket::close()
{
    if (!is_open()) return;

    _socket_impl->close();
    _socket_impl = nullptr;
}

socket::~socket()
{
    if (is_open()) _socket_impl->close();
}
