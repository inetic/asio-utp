#include <utp/socket.hpp>
#include "namespaces.hpp"
#include "socket_impl.hpp"


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

void socket::do_connect(const endpoint_type& ep, function<connect_signature> h)
{
    _socket_impl->do_connect(ep, std::move(move(h)));
}

void socket::do_accept(function<accept_signature> h)
{
    _socket_impl->do_accept(std::move(h));
}

void socket::do_write(function<write_signature> h)
{
    _socket_impl->do_send(std::move(h));
}

void socket::do_read(std::function<read_signature> h)
{
    _socket_impl->do_receive(std::move(h));
}

std::vector<boost::asio::const_buffer>& socket::tx_buffers()
{
    return _socket_impl->_tx_buffers;
}

std::vector<boost::asio::mutable_buffer>& socket::rx_buffers()
{
    return _socket_impl->_rx_buffers;
}
