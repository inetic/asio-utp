#include <asio_utp/udp_multiplexer.hpp>
#include "udp_multiplexer_impl.hpp"
#include "service.hpp"

using namespace std;
using namespace asio_utp;

struct udp_multiplexer::state {
    udp_multiplexer_impl::recv_entry recv_entry;

    udp_multiplexer::endpoint_type* rx_ep = nullptr;

    handler<size_t> tx_handler;
    handler<size_t> rx_handler;

    vector<asio::mutable_buffer> rx_buffers;
    vector<asio::const_buffer>   tx_buffers;

    std::shared_ptr<udp_multiplexer_impl> impl;

    void handle_read( const sys::error_code& ec
                    , const endpoint_type& ep
                    , const uint8_t* data
                    , size_t size) {
        if (!rx_handler) return;
        *rx_ep = ep;
        rx_ep = nullptr;
        size_t s = asio::buffer_copy(rx_buffers, asio::buffer(data, size));
        rx_handler.post(ec, s);
    }
};

udp_multiplexer::udp_multiplexer(boost::asio::io_context& ioc)
    : _ex(ioc.get_executor())
{}

udp_multiplexer::udp_multiplexer(const boost::asio::executor& ex)
    : _ex(ex)
{}

void udp_multiplexer::bind( const endpoint_type& local_ep
                          , sys::error_code& ec)
{
    using namespace std::placeholders;

    assert(!_state /* TODO: return error or rebind? */);
    sys::error_code ec_ignored;
    if (_state) close(ec_ignored);

    auto& ctx = _ex.context();

    auto impl = asio::use_service<service>(ctx)
        .maybe_create_udp_multiplexer(_ex, local_ep, ec);

    if (ec) return;

    _state = make_shared<state>();

    _state->impl = move(impl);

    _state->recv_entry.handler
        = std::bind(&state::handle_read, _state, _1, _2, _3, _4);
}

void udp_multiplexer::bind( const udp_multiplexer& other
                          , sys::error_code& ec)
{
    using namespace std::placeholders;

    assert(other._state);
    assert(other._state->impl);

    assert(!_state /* TODO: return error or rebind? */);
    sys::error_code ec_ignored;
    if (_state) close(ec_ignored);

    _state = make_shared<state>();
    _state->impl = other._state->impl;

    _state->recv_entry.handler
        = std::bind(&state::handle_read, _state, _1, _2, _3, _4);
}

shared_ptr<udp_multiplexer_impl> udp_multiplexer::impl() const
{
    assert(_state);
    return _state->impl;
}

void udp_multiplexer::do_send(const endpoint_type& dst, handler<size_t>&& h)
{
    if (!_state) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    auto& impl = *_state->impl;

    _state->tx_handler = move(h);

    impl.async_send_to(_state->tx_buffers, dst,
        [s = _state] (const sys::error_code& ec, size_t size) mutable {
            if (!s->tx_handler) return;
            s->tx_handler.post(ec, size);
        });
}

void udp_multiplexer::do_receive(endpoint_type& ep, handler<size_t>&& h)
{
    if (!_state) {
        return h.post(asio::error::bad_descriptor, 0);
    }

    assert(!_state->rx_handler && "Only one receive operation is "
            "allowed at a time");

    _state->rx_ep = &ep;
    _state->rx_handler = move(h);
    _state->impl->register_recv_handler(_state->recv_entry);
}

udp_multiplexer::on_send_to_connection udp_multiplexer::on_send_to(std::function<on_send_to_handler> handler)
{
    assert(_state);
    return _state->impl->on_send_to(std::move(handler));
}

udp_multiplexer::endpoint_type udp_multiplexer::local_endpoint() const
{
    assert(_state);
    return _state->impl->local_endpoint();
}

bool udp_multiplexer::is_open() const
{
    return bool(_state);
}

void udp_multiplexer::close(boost::system::error_code& ec)
{
    if (!_state) {
        ec = asio::error::bad_descriptor;
        return;
    }

    // Handler holds a shared_ptr to the _state, so reset it to avoid memory
    // leaks.
    _state->recv_entry.handler = nullptr;

    if (_state->rx_handler) {
        _state->rx_handler.post(asio::error::operation_aborted, 0);
    }

    if (_state->tx_handler) {
        _state->tx_handler.post(asio::error::operation_aborted, 0);
    }

    // `_state` may be kept from being destroyed by handlers, so make sure we
    // don't unnecessarily keep the udp_multiplexer_impl from being destroyed
    // as well.
    _state->impl = nullptr;

    _state = nullptr;
}

udp_multiplexer::~udp_multiplexer()
{
    sys::error_code ec;
    close(ec);
}

vector<asio::mutable_buffer>* udp_multiplexer::rx_buffers()
{
    if (!_state) return nullptr;
    return &_state->rx_buffers;
}

vector<asio::const_buffer>* udp_multiplexer::tx_buffers()
{
    if(!_state) return nullptr;
    return &_state->tx_buffers;
}
