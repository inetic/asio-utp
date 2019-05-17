#include <asio_utp/socket.hpp>
#include "service.hpp"
#include "../context.hpp"
#include "../util.hpp"

#include <utp.h>

using namespace std;
using namespace asio_utp;

socket_impl::socket_impl(boost::asio::io_context& ioc)
    : _ioc(ioc)
    , _service(asio::use_service<service>(_ioc.get_executor().context()))
    , _utp_socket(nullptr)
{}


void socket_impl::bind(const endpoint_type& ep)
{
    assert(!_context);
    _context = _service.get_or_create(_ioc, ep);
    _context->increment_use_count();
}


void socket_impl::on_connect()
{
    _connect_handler.post(sys::error_code());
}


void socket_impl::on_receive(const unsigned char* buf, size_t size)
{
    using asio::const_buffer;
    using asio::mutable_buffer;
    using asio::buffer_cast;
    using asio::buffer_size;
    using asio::buffer_copy;

    if (!_recv_handler) {
        _rx_buffer_queue.push_back({buf, buf+size});
        return;
    }

    assert(_rx_buffer_queue.empty()); 

    const_buffer src(buf, size);

    size_t total = 0;

    for (mutable_buffer dst : _rx_buffers) {
        size_t c = buffer_copy(dst, src);
        src = src + c;
        total += c;

        // If the recv buffer ir smaller than what we've received,
        // we need to store it for later.
        if (buffer_size(src) != 0) {
            const unsigned char* begin = buffer_cast<const unsigned char*>(src);
            const unsigned char* end   = begin + buffer_size(src);
            _rx_buffer_queue.push_back({begin, end});
            break;
        }
    }

    if (total == size) {
        utp_read_drained((utp_socket*) _utp_socket);
    }

    _recv_handler.post(sys::error_code(), total);
}


void socket_impl::on_accept(void* usocket)
{
    assert(!_utp_socket);
    assert(_accept_handler);

    utp_set_userdata((utp_socket*) usocket, this);

    _utp_socket = usocket;
    _accept_handler.dispatch(sys::error_code());
}


void socket_impl::do_write(handler<size_t> h)
{
    assert(!_send_handler);

    if (!_utp_socket) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    _send_handler = move(h);

    bool still_writable = true;

    for (auto& b : _tx_buffers) {
        while (size_t s = asio::buffer_size(b)) {
            // TODO: Use utp_writev
            auto w = utp_write( (utp_socket*) _utp_socket
                              , (void*) asio::buffer_cast<const void*>(b)
                              , s);

            _bytes_sent += w;
            b = b + w;
            s = asio::buffer_size(b);

            if (w < s) {
                still_writable = false;
                break;
            }
        }

        if (!still_writable) break;
    }

    if (still_writable) {
        _send_handler.post(sys::error_code(), _bytes_sent);
        _bytes_sent = 0;
    }
}


void socket_impl::on_writable()
{
    if (!_send_handler) return;
    do_write(move(_send_handler));
}

template<class Bufs>
static size_t buffers_size(const Bufs& bufs) {
    size_t ret = 0;
    for (auto& b : bufs) { ret += asio::buffer_size(b); }
    return ret;
}

void socket_impl::do_read(handler<size_t> h)
{
    assert(!_recv_handler);

    if (!_context) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    if (buffers_size(_rx_buffers) == 0) {
        return h.post(sys::error_code(), 0);
    }

    _recv_handler = std::move(h);

    // If we haven't yet received anything, we wait. But note that if we did,
    // but the _rx_buffers is empty, then we still post the callback with zero
    // size.
    if (_rx_buffer_queue.empty()) {
        return;
    }

    size_t s = asio::buffer_copy(_rx_buffers, _rx_buffer_queue);
    size_t r = s;

    while (r) {
        assert(!_rx_buffer_queue.empty());

        auto& buf = _rx_buffer_queue.front();

        if (r >= buf.size() - buf.consumed) {
            r -= buf.size() - buf.consumed;
            _rx_buffer_queue.erase(_rx_buffer_queue.begin());
        } else {
            buf.consumed += r;
            break;
        }
    }

    _recv_handler.post(sys::error_code(), s);
}


void socket_impl::do_accept(handler<> h)
{
    // TODO: Which error code to call `h` with?
    assert(_context);
    assert(!_accept_handler);
    _context->_accepting_sockets.push_back(*this);
    _accept_handler = move(h);
}


asio::ip::udp::endpoint socket_impl::local_endpoint() const
{
    assert(_context);
    return _context->udp_socket().local_endpoint();
}


void socket_impl::close()
{
    if (_closed) return;

    _closed = true;

    close_with_error(asio::error::operation_aborted);
}


void socket_impl::on_eof()
{
    close_with_error(asio::error::connection_reset);
}


void socket_impl::on_destroy()
{
    _utp_socket = nullptr;

    if (_context) {
        _context->decrement_use_count();
        _context = nullptr;
    }

    close_with_error(asio::error::connection_aborted);

    // Do this last as it may trigger the destructor.
    _self = nullptr;
}


void socket_impl::close_with_error(const sys::error_code& ec)
{
    if (_utp_socket) {
        utp_close((utp_socket*) _utp_socket);
        _self = shared_from_this();
    }

    if (_accept_handler) {
        _accept_handler.post(ec);
    }

    if (_connect_handler) {
        _connect_handler.post(ec);
    }

    if (_recv_handler) {
        _recv_handler.post(ec, 0);
    }

    if (_send_handler) {
        _send_handler.post(ec, 0);
    }
}


socket_impl::~socket_impl()
{
    if (_utp_socket) {
        utp_set_userdata((utp_socket*) _utp_socket, nullptr);
    }
    else {
        // _utp_socket is null, so on_destroy won't be called from libutp
        on_destroy();
    }
}


void socket_impl::do_connect(const endpoint_type& ep, handler<> h)
{
    assert(!_utp_socket);

    _connect_handler = move(h);

    sockaddr_storage addr = util::to_sockaddr(ep);

    _utp_socket = utp_create_socket(_context->get_libutp_context());
    utp_set_userdata((utp_socket*) _utp_socket, this);

    utp_connect((utp_socket*) _utp_socket, (sockaddr*) &addr, util::sockaddr_size(addr));
}
