#include <utp/socket.hpp>
#include "udp_loop.hpp"
#include "util.hpp"

#include <utp.h>

using namespace utp;
using namespace std;

socket::socket(boost::asio::io_service& ios)
    : _ios(ios)
    , _utp_socket(nullptr)
{}

socket::socket(shared_ptr<udp_loop> ul, void* us)
    : _ios(ul->get_io_service())
    , _utp_socket(us)
    , _udp_loop(move(ul))
{}

socket::socket(socket&& other)
    : _ios(other._ios)
    , _utp_socket(other._utp_socket)
    , _udp_loop(move(other._udp_loop))
{
    other._utp_socket = nullptr;

    if (_utp_socket) {
        utp_set_userdata((utp_socket*) _utp_socket, this);
    }
}


void socket::bind(const endpoint_type& ep)
{
    asio::ip::udp::socket s(_ios, ep);
    _udp_loop = make_shared<udp_loop>(move(s));
    _udp_loop->_use_count++;
    _udp_loop->start();
}


void socket::on_connect()
{
    _ios.post([h = move(_connect_handler)] { h(sys::error_code()); });
}


void socket::on_writable()
{
    cerr << this << " socket::on_writable()" << endl;
}


void socket::on_receive(const unsigned char* buf, size_t size)
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

    _ios.post([total, h = move(_recv_handler)] {
        h(sys::error_code(), total);
    });
}


void socket::on_eof()
{
    close_with_error(asio::error::connection_reset);
}


void socket::on_destroy()
{
    // Set it to nullptr so that we won't call utp_close on it again.
    _utp_socket = nullptr;
    close_with_error(asio::error::connection_aborted);
}


void socket::on_accept(void* usocket)
{
    assert(!_utp_socket);
    assert(_accept_handler);

    utp_set_userdata((utp_socket*) usocket, this);

    _utp_socket = usocket;
    auto h = move(_accept_handler);
    h(sys::error_code());
}


void socket::do_send(send_handler_type&& h)
{
    assert(!_send_handler);
    assert(_utp_socket);

    _send_handler = [ w = asio::io_service::work(_ios)
                    , h = std::move(h)]
                    (const sys::error_code& ec, size_t size) {
                        h(ec, size);
                    };

    bool still_writable = true;

    _bytes_sent = 0;

    for (auto b : _tx_buffers) {
        size_t s = asio::buffer_size(b);

        if (s == 0) continue;

        // TODO: Use utp_writev
        auto w = utp_write( (utp_socket*) _utp_socket
                          , (void*) asio::buffer_cast<const void*>(b)
                          , s);

        if (w == 0) {
            still_writable = false;
            break;
        }

        _bytes_sent += w;
        b = b + w;
    }

    if (still_writable) {
        _ios.post([h = move(_send_handler), c = _bytes_sent] {
                h(sys::error_code(), c);
            });

        _bytes_sent = 0;
    }
}


void socket::do_receive(recv_handler_type&& h)
{
    assert(!_recv_handler);

    if (!_udp_loop) {
        return _ios.post([h = move(h)] { h(asio::error::bad_descriptor, 0); });
    }

    _recv_handler = [ w = asio::io_service::work(_ios)
                    , h = std::move(h)]
                    (const sys::error_code& ec, size_t size) {
                        h(ec, size);
                    };

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
            // TODO: This is inefficient.
            _rx_buffer_queue.erase(_rx_buffer_queue.begin());
        } else {
            buf.consumed += r;
            break;
        }
    }

    _ios.post([s, h = move(_recv_handler)] { h(sys::error_code(), s); });
}


void socket::do_accept(accept_handler_type&& h)
{
    // TODO: Which error code to call `h` with?
    assert(_udp_loop);
    assert(!_accept_handler);

    _udp_loop->_accepting_sockets.push_back(*this);

    _accept_handler = [ w = asio::io_service::work(_ios)
                      , h = move(h)
                      ] (const sys::error_code& ec) { h(ec); };
}


asio::ip::udp::endpoint socket::local_endpoint() const
{
    assert(_udp_loop);
    return _udp_loop->udp_socket().local_endpoint();
}


void socket::close()
{
    close_with_error(asio::error::operation_aborted);
}

void socket::close_with_error(const sys::error_code& ec)
{
    if (_utp_socket) {
        utp_close((utp_socket*) _utp_socket);
        _utp_socket = nullptr;
    }

    if (_udp_loop) {
        if (--_udp_loop->_use_count == 0) {
            _udp_loop->stop();
        }

        _udp_loop = nullptr;
    }

    if (_accept_handler) {
        _ios.post([h = move(_accept_handler), ec] { h(ec); });
    }

    if (_connect_handler) {
        _ios.post([h = move(_connect_handler), ec] { h(ec); });
    }

    if (_recv_handler) {
        _ios.post([h = move(_recv_handler), ec] { h(ec, 0); });
    }
}


socket::~socket()
{
    close();
}


void socket::do_connect(const endpoint_type& ep)
{
    assert(!_utp_socket);

    sockaddr addr = util::to_sockaddr(ep);

    _utp_socket = utp_create_socket(_udp_loop->get_utp_context());
    utp_set_userdata((utp_socket*) _utp_socket, this);

    utp_connect((utp_socket*) _utp_socket, &addr, sizeof(addr));
}
