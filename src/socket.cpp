#include <asio_utp/socket.hpp>
#include "namespaces.hpp"
#include "socket_impl.hpp"

using namespace std;
using namespace asio_utp;

socket::socket(boost::asio::io_context& ioc)
    : _ioc(&ioc)
{}

void socket::bind(const endpoint_type& ep, sys::error_code& ec)
{
    if (_socket_impl) {
        // TODO: Is this the correct error code?
        ec = asio::error::already_open;
        return;
    }

    _socket_impl = make_shared<socket_impl>(this);
    _socket_impl->bind(ep);
}

void socket::bind(const udp_multiplexer& m, sys::error_code& ec)
{
    if (_socket_impl) {
        // TODO: Is this the correct error code?
        ec = asio::error::already_open;
        return;
    }

    _socket_impl = make_shared<socket_impl>(this);
    _socket_impl->bind(m);
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

    _ioc = other._ioc;
    _socket_impl = move(other._socket_impl);

    if (_socket_impl) {
        assert(other._socket_impl->_owner);
        _socket_impl->_owner = this;
    }

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

void socket::do_connect(const endpoint_type& ep_, handler<>&& h)
{
    if (!_socket_impl) {
        return h.post(asio::error::bad_descriptor);
    }

    auto ep = ep_;

    // Libutp can't connect to an unspecified IP address. But it seems
    // (https://tools.ietf.org/html/rfc5735#section-3) it's OK if we connect to
    // "this" host instead.
    if (ep.address().is_unspecified()) {
        if (ep.address().is_v4()) {
            ep.address(asio::ip::address_v4::loopback());
        } else {
            ep.address(asio::ip::address_v6::loopback());
        }
    }

    _socket_impl->do_connect(ep, std::move(move(h)));
}

void socket::do_accept(handler<>&& h)
{
    if (!_socket_impl) {
        return h.post(asio::error::bad_descriptor);
    }

    _socket_impl->do_accept(std::move(h));
}

void socket::do_write(handler<size_t>&& h)
{
    if (!_socket_impl) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    _socket_impl->do_write(std::move(h));
}

void socket::do_read(handler<size_t>&& h)
{
    if (!_socket_impl) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    _socket_impl->do_read(std::move(h));
}

std::vector<boost::asio::const_buffer>* socket::tx_buffers()
{
    if (!_socket_impl) return nullptr;
    return &_socket_impl->_tx_buffers;
}

std::vector<boost::asio::mutable_buffer>* socket::rx_buffers()
{
    if (!_socket_impl) return nullptr;
    return &_socket_impl->_rx_buffers;
}
