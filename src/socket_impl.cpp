#include <asio_utp/socket.hpp>
#include <asio_utp/log.hpp>
#include <asio_utp/udp_multiplexer.hpp>
#include "service.hpp"
#include "context.hpp"
#include "util.hpp"
#include "weak_from_this.hpp"

#include <utp.h>

using namespace std;
using namespace asio_utp;

socket_impl::socket_impl(socket* owner)
    : _ioc(owner->get_io_service())
    , _service(asio::use_service<service>(_ioc.get_executor().context()))
    , _owner(owner)
{
    static decltype(_debug_id) next_debug_id = 1;
    _debug_id = next_debug_id++;

    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::socket_impl()");
    }
}


void socket_impl::bind(const endpoint_type& ep)
{
    assert(!_context);
    _context = _service.maybe_create_context(_ioc, ep);

    if (_debug) {
        log(this, " socket_impl::bind() _context:", _context);
    }

    _context->increment_use_count();
}

void socket_impl::bind(const udp_multiplexer& m)
{
    assert(!_context);
    _context = _service.maybe_create_context(m.impl());

    if (_debug) {
        log(this, " socket_impl::bind() _context:", _context);
    }

    _context->increment_use_count();
}

void socket_impl::on_connect()
{
    post_op(_connect_handler, "connect", sys::error_code());
}


void socket_impl::on_receive(const unsigned char* buf, size_t size)
{
    if (_debug) {
        log( this, " debug_id:", _debug_id, " socket_impl::on_receive "
           , "_recv_handler:", bool(_recv_handler), " "
           , "size:", size);
    }

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

        // If the recv buffer is smaller than what we've received,
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

    post_op(_recv_handler, "recv", sys::error_code(), total);
}


void socket_impl::on_accept(void* usocket)
{
    if (_debug) {
        log(this, " socket_impl::on_accept utp_socket:", usocket);
    }

    assert(!_utp_socket);
    assert(_accept_handler);

    utp_set_userdata((utp_socket*) usocket, this);

    _utp_socket = usocket;
    dispatch_op(_accept_handler, "accept", sys::error_code());
}


template<class Handler>
void socket_impl::setup_op(Handler& target, Handler&& h, const char* dbg)
{
    _context->increment_outstanding_ops(dbg);
    target = move(h);
    target.exec_after([ctx = _context, dbg] { ctx->decrement_completed_ops(dbg); });
}

template<class Handler, class... Args>
void socket_impl::post_op(Handler& h, const char* dbg, const sys::error_code& ec, Args... args)
{
    _context->increment_completed_ops(dbg);
    _context->decrement_outstanding_ops(dbg);
    h.post(ec, args...);
}

template<class Handler, class... Args>
void socket_impl::dispatch_op(Handler& h, const char* dbg, const sys::error_code& ec, Args... args)
{
    _context->increment_completed_ops(dbg);
    _context->decrement_outstanding_ops(dbg);
    h.dispatch(ec, args...);
}

void socket_impl::do_write(handler<size_t> h)
{
    if (_debug) {
        log(this, " socket_impl::do_write");
    }

    assert(!_send_handler);

    if (!_utp_socket) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    setup_op(_send_handler, move(h), "write");

    bool still_writable = true;

    for (auto& b : _tx_buffers) {
        while (size_t s = asio::buffer_size(b)) {
            // TODO: Use utp_writev
            auto w = utp_write( (utp_socket*) _utp_socket
                              , (void*) asio::buffer_cast<const void*>(b)
                              , s);

            assert(w >= 0);

            _bytes_sent += w;
            b = b + w;
            s = asio::buffer_size(b);

            if (size_t(w) < s) {
                still_writable = false;
                break;
            }
        }

        if (!still_writable) break;
    }

    if (still_writable) {
        post_op(_send_handler, "write", sys::error_code(), _bytes_sent);
        _bytes_sent = 0;
    }
}


void socket_impl::on_writable()
{
    if (_debug) {
        log(this, " socket_impl::on_writable");
    }

    if (!_send_handler) return;
    do_write(move(_send_handler));
}

void socket_impl::do_read(handler<size_t> h)
{
    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::do_read ",
            " buffer_size(_rx_buffers):", asio::buffer_size(_rx_buffers),
            " _rx_buffer_queue.size():", _rx_buffer_queue.size(),
            " buffer_size(_rx_buffer_queue):", asio::buffer_size(_rx_buffer_queue));
    }

    assert(!_recv_handler);

    if (!_context) {
        // User provided an empty RX buffer => post handler right a way.
        return h.post(asio::error::bad_descriptor, 0);
    }

    if (asio::buffer_size(_rx_buffers) == 0) {
        return h.post(sys::error_code(), 0);
    }

    setup_op(_recv_handler, move(h), "read");

    // If we haven't yet received anything, we wait. But note that if we did,
    // but the _rx_buffers is empty, then we still post the callback with zero
    // size.
    if (_rx_buffer_queue.empty()) {
        if (_got_eof) {
            close_with_error(asio::error::connection_reset);
        }
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

    post_op(_recv_handler, "recv", sys::error_code(), s);
}


void socket_impl::do_accept(handler<> h)
{
    if (_debug) {
        log(this, " socket_impl::do_accept");
    }

    // TODO: Which error code to call `h` with?
    assert(_context);
    assert(!_accept_handler);
    _context->_accepting_sockets.push_back(*this);

    setup_op(_accept_handler, move(h), "accept");
}


asio::ip::udp::endpoint socket_impl::local_endpoint() const
{
    assert(_context);
    return _context->local_endpoint();
}


void socket_impl::close()
{
    if (_debug) {
        log(this, " socket_impl::close()");
    }

    if (_closed) return;

    _closed = true;

    close_with_error(asio::error::operation_aborted);
}


void socket_impl::on_eof()
{
    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::on_eof",
                " _send_handler:", bool(_send_handler),
                " _recv_handler:", bool(_recv_handler));
    }

    assert(!_got_eof);
    _got_eof = true;

    if (_recv_handler) {
        post_op(_recv_handler, "recv", asio::error::connection_reset, 0);
    }
}


// Called by libutp once the socket finished its termination sequence (send
// `fin`; receive `ack`; etc...)
void socket_impl::on_destroy()
{
    if (_debug) {
        log( this, " debug_id:", _debug_id, " socket_impl::on_destroy"
           , " refcount:", asio_utp::weak_from_this(this).use_count()
           , " _self:", _self.get());
    }

    assert(_utp_socket);

    _utp_socket = nullptr;

    close_with_error(asio::error::connection_aborted);

    if (_self) {
        _context->decrement_outstanding_ops("close");
    }

    // This function is called from inside libutp. We must make sure that
    // neither _utp_socket, nor the _context get destroyed before that function
    // finishes. On the other hand we do want to schedule destruction of `this`
    // some time after that.
    get_executor().post( [&, s = shared_from_this()] { _self = nullptr; }
            , std::allocator<void>());
}


void socket_impl::close_with_error(const sys::error_code& ec)
{
    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::close_with_error "
            "_utp_socket:", _utp_socket, " _self:", _self.get());
    }

    if (_utp_socket) {
        utp_close((utp_socket*) _utp_socket);
        _self = shared_from_this();
        if (_owner) {
            _owner->_socket_impl = nullptr;
            _owner = nullptr;
        }
        _context->increment_outstanding_ops("close");
    }

    if (_accept_handler) {
        post_op(_accept_handler, "accept", ec);
    }

    if (_connect_handler) {
        post_op(_connect_handler, "connect", ec);
    }

    if (_recv_handler) {
        post_op(_recv_handler, "recv", ec, 0);
    }

    if (_send_handler) {
        post_op(_send_handler, "send", ec, 0);
    }
}


socket_impl::~socket_impl()
{
    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::~socket_impl()");
    }

    if (_utp_socket) {
        utp_set_userdata((utp_socket*) _utp_socket, nullptr);
    }

    close_with_error(asio::error::connection_aborted);

    if (_context) {
        _context->decrement_use_count();
    }
}


void socket_impl::do_connect(const endpoint_type& ep, handler<> h)
{
    if (_debug) {
        log(this, " debug_id:", _debug_id, " socket_impl::do_connect ep:", ep);
    }

    assert(!_utp_socket);

    setup_op(_connect_handler, move(h), "connect");

    sockaddr_storage addr = util::to_sockaddr(ep);

    _utp_socket = utp_create_socket(_context->get_libutp_context());
    utp_set_userdata((utp_socket*) _utp_socket, this);

    utp_connect((utp_socket*) _utp_socket, (sockaddr*) &addr, util::sockaddr_size(addr));
}
