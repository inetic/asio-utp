#include "context.hpp"
#include "service.hpp"
#include <asio_utp/socket.hpp>
#include <boost/asio/steady_timer.hpp>

#include <iostream>

using namespace std;
using namespace asio_utp;

struct context::ticker_type : public enable_shared_from_this<ticker_type> {
    bool _running = false;
    bool _outstanding = false;
    asio::steady_timer _timer;
    function<void()> _on_tick;

#if BOOST_VERSION >= 107000
    ticker_type(asio::executor&& ex, function<void()> on_tick)
        : _timer(move(ex))
        , _on_tick(move(on_tick))
    {
    }
#else
    ticker_type(asio::io_context::executor_type&& ex, function<void()> on_tick)
        : _timer(ex.context())
        , _on_tick(move(on_tick))
    {
    }
#endif

    void start() {
        if (_running) return;
        _running = true;
        if (_outstanding) return;
        _timer.expires_after(chrono::milliseconds(500));
        _outstanding = true;
        _timer.async_wait([this, self = shared_from_this()]
                          (const sys::error_code& ec) {
                              _outstanding = false;
                              if (!_running) return;
                              _on_tick();
                              if (!_running) return;
                              _running = false;
                              start();
                          });
    }

    void stop() {
        if (!_running) return;
        _running = false;
        _timer.cancel();
    }

    ~ticker_type() {
        stop();
    }
};

uint64 context::callback_log(utp_callback_arguments* a)
{
    cerr << "LOG: " << a->socket << " " << a->buf << endl;
    return 0;
}

uint64 context::callback_sendto(utp_callback_arguments* a)
{
    context* self = (context*) utp_context_get_userdata(a->context);

    sys::error_code ec;

    self->_multiplexer->send_to( asio::buffer(a->buf, a->len)
                               , util::to_endpoint(*a->address)
                               , 0
                               , ec);

    // The libutp library sometimes calls this function even after the last
    // socket holding this context has received an EOF and closed.
    // TODO: Should this be fixed in libutp?
    if (ec && ec == asio::error::bad_descriptor) {
        return 0;
    }

    if (ec && ec != asio::error::would_block) {
        assert(0 && "TODO");
    }

    return 0;
}

uint64 context::callback_on_error(utp_callback_arguments*)
{
    return 0;
}

static const char* libutp_state_name(int state) {
    switch(state) {
        case UTP_STATE_CONNECT:    return "UTP_STATE_CONNECT";
        case UTP_STATE_WRITABLE:   return "UTP_STATE_WRITABLE";
        case UTP_STATE_EOF:        return "UTP_STATE_EOF";
        case UTP_STATE_DESTROYING: return "UTP_STATE_DESTROYING";
        default:                   return "UNKNOWN";
    }
}

uint64 context::callback_on_state_change(utp_callback_arguments* a)
{
    auto socket = (socket_impl*) utp_get_userdata(a->socket);

    auto* ctx = socket ? socket->_context.get() : nullptr;

    if (ctx->_debug) {
        cerr << ctx << " context::callback_on_state_change"
           << " socket:" << socket
           << " new_state:" << libutp_state_name(a->state)
           << "\n";
    }

    if (!socket) {
        // The utp::socket_impl has detached from this utp_socket
        return 0;
    }

    switch(a->state) {
        case UTP_STATE_CONNECT:
            socket->on_connect();
            break;

        case UTP_STATE_WRITABLE:
            socket->on_writable();
            break;

        case UTP_STATE_EOF:
            socket->on_eof();
            break;

        case UTP_STATE_DESTROYING:
            socket->on_destroy();
            break;
    }

    return 0;
}

uint64 context::callback_on_read(utp_callback_arguments* a)
{
    auto socket = (socket_impl*) utp_get_userdata(a->socket);
    assert(socket);
    socket->on_receive(a->buf, a->len);

    return 0;
}

uint64 context::callback_on_firewall(utp_callback_arguments* a)
{
    auto* self = (context*) utp_context_get_userdata(a->context);

    if (self->_accepting_sockets.empty()) {
        return 1;
    }

    return 0;
}

uint64 context::callback_on_accept(utp_callback_arguments* a)
{
    auto* self = (context*) utp_context_get_userdata(a->context);

    if (self->_accepting_sockets.empty()) return 0;

    auto& s = self->_accepting_sockets.front();
    self->_accepting_sockets.pop_front();

    s.on_accept(a->socket);

    return 0;
}

context::context(shared_ptr<udp_multiplexer_impl> m)
    : _multiplexer(std::move(m))
    , _local_endpoint(_multiplexer->local_endpoint())
    , _utp_ctx(utp_init(2 /* version */))
{
    if (_debug) {
        cerr << this << " context::context()\n";
    }

    // TODO: Throw?
    assert(_utp_ctx);

    _recv_handle.handler = [&] ( const sys::error_code& ec
                               , const endpoint_type& ep
                               , const vector<uint8_t>& data) {
        return on_read(ec, ep, data);
    };

    _ticker = make_shared<ticker_type>(get_executor(), [this] {
            assert(_utp_ctx);
            if (!_utp_ctx) return;
            if (_debug) {
                cerr << this << " context on_tick\n";
            }
            utp_check_timeouts(_utp_ctx);
        });

    utp_context_set_userdata(_utp_ctx, this);

#if ASIO_UTP_DEBUG_LOGGING
    utp_set_callback(_utp_ctx, UTP_LOG,             &callback_log);
    //utp_context_set_option(_utp_ctx, UTP_LOG_MTU,    1);
    utp_context_set_option(_utp_ctx, UTP_LOG_NORMAL, 1);
    utp_context_set_option(_utp_ctx, UTP_LOG_DEBUG,  1);
#endif

    utp_set_callback(_utp_ctx, UTP_SENDTO,          &callback_sendto);
    utp_set_callback(_utp_ctx, UTP_ON_ERROR,        &callback_on_error);
    utp_set_callback(_utp_ctx, UTP_ON_STATE_CHANGE, &callback_on_state_change);
    utp_set_callback(_utp_ctx, UTP_ON_READ,         &callback_on_read);
    utp_set_callback(_utp_ctx, UTP_ON_FIREWALL,     &callback_on_firewall);
    utp_set_callback(_utp_ctx, UTP_ON_ACCEPT,       &callback_on_accept);
}

void context::increment_use_count()
{
    if (_use_count++ == 0) start();
}

void context::decrement_use_count()
{
    if (--_use_count == 0) stop();
}

void context::start_receiving()
{
    assert(_recv_handle.handler);
    _ticker->start();

    if (!_recv_handle.is_linked())
        _multiplexer->register_recv_handler(_recv_handle);
}

void context::start()
{
    if (_debug) {
        cerr << this << " context start\n";
    }
}

void context::stop()
{
    if (_debug) {
        cerr << this << " context stop\n";
    }

    _ticker->stop();
}

void context::on_read( const sys::error_code& read_ec
                     , const endpoint_type& ep
                     , const vector<uint8_t>& data)
{
    sys::error_code ec;

    if (!_multiplexer->available(ec)) {
        utp_issue_deferred_acks(_utp_ctx);
    }

    if (read_ec) return;

    sockaddr_storage src_addr = util::to_sockaddr(ep);

    // XXX: This returns a boolean whether the data were handled or not.
    // May be good to use it to decide whether to pass the data to other
    // multiplexers.
    utp_process_udp( _utp_ctx
                   , (unsigned char*) data.data()
                   , data.size()
                   , (sockaddr*) &src_addr
                   , util::sockaddr_size(src_addr));

    if (!_multiplexer->available(ec)) {
        utp_issue_deferred_acks(_utp_ctx);
    }

    if (_outstanding_op_count) start_receiving();
}

context::executor_type context::get_executor()
{
    assert(_multiplexer && "TODO");
    return _multiplexer->get_executor();
}

context::~context()
{
    if (_debug) {
        cerr << this << " ~context\n";
    }

    utp_destroy(_utp_ctx);

    auto& s = asio::use_service<service>(_multiplexer->get_executor().context());
    s.erase_context(_local_endpoint);
}

void context::increment_outstanding_ops(const char* dbg)
{
    if (_debug) {
        cerr << this << " context::increment_outstanding_ops "
            << _outstanding_op_count << " -> " << (_outstanding_op_count + 1)
            << " "  << dbg << " (completed:" << _completed_op_count << ")\n";
    }

    if (_outstanding_op_count++ == 0) {
        start_receiving();
    }
}

void context::decrement_outstanding_ops(const char* dbg)
{
    if (_debug) {
        cerr << this << " context::decrement_outstanding_ops "
            << _outstanding_op_count << " -> " << (_outstanding_op_count - 1)
            << " " << dbg << " (completed:" << _completed_op_count << ")\n";
    }

    if (--_outstanding_op_count == 0 && _completed_op_count == 0) {
        _ticker->stop();
    }
}

void context::increment_completed_ops(const char* dbg)
{
    if (_debug) {
        cerr << this << " context::increment_completed_ops "
            << _completed_op_count << " -> " << (_completed_op_count + 1)
            << " "  << dbg << " (outstanding:" << _outstanding_op_count << ")\n";
    }

    _completed_op_count++;
}

void context::decrement_completed_ops(const char* dbg)
{
    if (_debug) {
        cerr << this << " context::decrement_completed_ops "
            << _completed_op_count << " -> " << (_completed_op_count - 1)
            << " " << dbg << " (outstanding:" << _outstanding_op_count << ")\n";
    }

    if (--_completed_op_count == 0 && _outstanding_op_count == 0) {
        _ticker->stop();
    }
}
