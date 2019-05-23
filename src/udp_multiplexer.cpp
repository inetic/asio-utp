#include <asio_utp/udp_multiplexer.hpp>
#include "udp_multiplexer_impl.hpp"
#include "service.hpp"

using namespace std;
using namespace asio_utp;

struct udp_multiplexer::state {
    udp_multiplexer_impl::recv_entry recv_entry;

    udp_multiplexer::endpoint_type* rx_ep = nullptr;
    handler<size_t> rx_handler;
    vector<asio::mutable_buffer> rx_buffers;

    vector<asio::const_buffer> tx_buffers;

    std::shared_ptr<udp_multiplexer_impl> impl;
};

udp_multiplexer::udp_multiplexer( boost::asio::io_context& ioc
                                , const endpoint_type& ep)
    : _ioc(&ioc)
    , _state(make_shared<state>())
{
    _state->impl =
        asio::use_service<service>(ioc).maybe_create_udp_multiplexer(ioc, ep);

    _state->recv_entry.handler =
        [s = _state] ( const sys::error_code& ec
                     , const endpoint_type& ep
                     , const std::vector<uint8_t>& v)
        {
            *s->rx_ep = ep;
            s->rx_ep = nullptr;
            size_t size = asio::buffer_copy(s->rx_buffers, asio::buffer(v));
            s->rx_handler.post(ec, size);
        };
}

void udp_multiplexer::do_send(const endpoint_type& dst, handler<size_t>&& h)
{
    assert(_state);

    auto& impl = *_state->impl;

    impl.async_send_to(_state->tx_buffers, dst,
        [h = move(h)] (const sys::error_code& ec, size_t size) mutable {
            h.post(ec, size);
        });
}

void udp_multiplexer::do_receive(endpoint_type& ep, handler<size_t>&& h)
{
    assert(_state);

    auto& impl = *_state->impl;

    assert(!_state->rx_handler && "Only one receive operation is "
            "allowed at a time");

    _state->rx_ep = &ep;
    _state->rx_handler = move(h);
    _state->impl->register_recv_handler(_state->recv_entry);
}

udp_multiplexer::endpoint_type udp_multiplexer::local_endpoint() const
{
    assert(_state);
    return _state->impl->local_endpoint();
}

vector<asio::mutable_buffer>& udp_multiplexer::rx_buffers()
{
    assert(_state);
    return _state->rx_buffers;
}

vector<asio::const_buffer>& udp_multiplexer::tx_buffers()
{
    assert(_state);
    return _state->tx_buffers;
}
