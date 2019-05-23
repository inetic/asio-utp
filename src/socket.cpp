#include <asio_utp/socket.hpp>
#include "namespaces.hpp"
#include "socket_impl.hpp"

using namespace std;
using namespace asio_utp;

socket::socket(boost::asio::io_context& ioc, const endpoint_type& ep)
    : _ioc(&ioc)
    , _socket_impl(make_shared<socket_impl>(this))
{
    _socket_impl->bind(ep);
}

socket::socket(socket&& other)
    : _ioc(other._ioc)
    , _socket_impl(move(other._socket_impl))
{
    other._ioc = nullptr;
    _socket_impl->_owner = this;
}

asio_utp::socket& socket::operator=(socket&& other)
{
    assert(!_ioc || !other._ioc || _ioc == other._ioc);
    assert(other._socket_impl->_owner);
    _ioc = other._ioc;
    _socket_impl = move(other._socket_impl);
    _socket_impl->_owner = this;
    return *this;
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

void socket::do_connect(const endpoint_type& ep, handler<>&& h)
{
    _socket_impl->do_connect(ep, std::move(move(h)));
}

void socket::do_accept(handler<>&& h)
{
    _socket_impl->do_accept(std::move(h));
}

void socket::do_write(handler<size_t>&& h)
{
    _socket_impl->do_write(std::move(h));
}

void socket::do_read(handler<size_t>&& h)
{
    _socket_impl->do_read(std::move(h));
}

std::vector<boost::asio::const_buffer>& socket::tx_buffers()
{
    assert(_socket_impl);
    return _socket_impl->_tx_buffers;
}

std::vector<boost::asio::mutable_buffer>& socket::rx_buffers()
{
    assert(_socket_impl);
    return _socket_impl->_rx_buffers;
}
