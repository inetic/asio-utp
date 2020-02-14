#pragma once

#include <boost/asio/ip/udp.hpp>
#include <boost/intrusive/list.hpp>
#include "namespaces.hpp"
#include "weak_from_this.hpp"
#include <asio_utp/log.hpp>
#include <asio_utp/detail/signal.hpp>
#include <iostream>

namespace asio_utp {

class udp_multiplexer_impl
    : public std::enable_shared_from_this<udp_multiplexer_impl>
{
public:
    using endpoint_type = asio::ip::udp::endpoint;

    using on_send_to_handler = void(
        const std::vector<boost::asio::const_buffer>&,
        size_t,
        const endpoint_type&,
        boost::system::error_code
    );
    using on_send_to_connection = Signal<on_send_to_handler>::Connection;

    using handler_type = std::function<void( const sys::error_code&
                                           , const endpoint_type&
                                           , const std::vector<uint8_t>&)>;

private:
    using intrusive_hook = boost::intrusive::list_base_hook
        <boost::intrusive::link_mode
            <boost::intrusive::auto_unlink>>;

public:
    struct recv_entry : intrusive_hook {
        std::weak_ptr<udp_multiplexer_impl> multiplexer;
        handler_type handler;

        ~recv_entry();
    };

private:
    using recv_handlers = boost::intrusive::list
        < recv_entry
        , boost::intrusive::constant_time_size<false>>;

public:
    udp_multiplexer_impl(asio::ip::udp::socket);

    std::size_t send_to( const std::vector<asio::const_buffer>&
                       , const endpoint_type& destination
                       , asio::socket_base::message_flags
                       , sys::error_code&);

    template< typename WriteHandler>
    void async_send_to( const std::vector<asio::const_buffer>&
                      , const endpoint_type&
                      , WriteHandler&&);

    void register_recv_handler(recv_entry&);

    on_send_to_connection on_send_to(std::function<on_send_to_handler> handler);

    endpoint_type local_endpoint() const {
        return _udp_socket.local_endpoint();
    }

#if BOOST_VERSION >= 107000
    boost::asio::executor get_executor()
#else
    boost::asio::io_context::executor_type get_executor()
#endif
    {
        return _udp_socket.get_executor();
    }

    bool is_open() const { return _udp_socket.is_open(); }

    size_t available(sys::error_code&) const;

    ~udp_multiplexer_impl();

private:
    void start_receiving();
    void flush_handlers(const sys::error_code& ec, size_t size);
    void on_recv_entry_unlinked();

    // For debugging only
    static
    std::string to_hex(uint8_t*, size_t);

private:
    struct State {
        endpoint_type rx_endpoint;
        std::vector<uint8_t> rx_buffer;
    };

    asio::ip::udp::socket _udp_socket;
    recv_handlers _recv_handlers;
    Signal<on_send_to_handler> _send_to_signal;
    std::shared_ptr<State> _state;
    bool _is_receiving = false;
    bool _debug = false;
};

} // asio_udp namespace

#include "service.hpp"

namespace asio_utp {

inline udp_multiplexer_impl::udp_multiplexer_impl(asio::ip::udp::socket s)
    : _udp_socket(std::move(s))
    , _state(std::make_shared<State>())
{
    if (_debug) {
        log(this, " udp_multiplexer_impl(", _udp_socket.local_endpoint(), ")");
    }

    if (!_udp_socket.non_blocking()) {
        _udp_socket.non_blocking(true);
    }
}

inline
udp_multiplexer_impl::recv_entry::~recv_entry()
{
    auto m = multiplexer.lock();
    assert(!is_linked() || m /* is_linked implies m */);
    if (!m) return;

    if (is_linked()) {
        unlink();
        m->on_recv_entry_unlinked();
    }
}

inline
void udp_multiplexer_impl::register_recv_handler(recv_entry& e)
{
    e.multiplexer = asio_utp::weak_from_this(this);
    _recv_handlers.push_back(e);

    if (!_is_receiving) {
        start_receiving();
    }
}

inline
void udp_multiplexer_impl::on_recv_entry_unlinked()
{
    if (_is_receiving && _recv_handlers.empty()) {
        // We need to do this to prevent this multiplexer from blocking
        // in the io_context.run function.

        // TODO: Unfortunately this won't work on Windows XP.
        // See "Remarks" section in Boost.Asio documentation for
        // basic_datagram_socket::cancel function.
        // Another drawback is that this will cancel other async
        // operations as well.
        //
        // A workaround I can think of right now is to either find
        // out whether it is possible to invoke async_receive_from
        // such that it doesn't block io_context.run or have another
        // socket (or perhaps the same one?) send this socket a
        // message to release from async_receive_from.
        sys::error_code ec;
        _udp_socket.cancel(ec);
        assert(!ec);
    }
}

inline void udp_multiplexer_impl::start_receiving()
{
    assert(!_is_receiving);
    _is_receiving = true;

    _state->rx_buffer.resize(65537);

    auto wself = asio_utp::weak_from_this(this);

    _udp_socket.async_receive_from
        ( asio::buffer(_state->rx_buffer)
        , _state->rx_endpoint
        , [&, wself, s = _state] (const sys::error_code& ec, size_t size)
          {
              if (auto self = wself.lock()) {
                  assert(_is_receiving);
                  assert(_state->rx_buffer.size() == 65537);

                  bool canceled = ec == asio::error::operation_aborted
                               && _udp_socket.is_open();

                  if (!canceled) {
                      flush_handlers(ec, size);
                  }

                  _is_receiving = false;

                  if (!_recv_handlers.empty()) {
                      start_receiving();
                  }
              }
          });
}

inline
void udp_multiplexer_impl::flush_handlers(const sys::error_code& ec, size_t size)
{
    if (_debug) {
        log(this, " udp_multiplexer::flush_handlers "
            "ec:", ec.message(), " size:", size, " from:", _state->rx_endpoint);
        if (!ec) {
            log(this, "    ", to_hex((uint8_t*)_state->rx_buffer.data(), size));
        }
    }

    if (ec) size = 0;

    _state->rx_buffer.resize(size);

    auto recv_handlers = std::move(_recv_handlers);

    while (!recv_handlers.empty()) {
        auto e = recv_handlers.front();
        recv_handlers.pop_front();
        assert(e.handler);
        e.handler(ec, _state->rx_endpoint, _state->rx_buffer);
    }
}

inline
std::size_t udp_multiplexer_impl::send_to( const std::vector<asio::const_buffer>& buffers
                                         , const endpoint_type& destination
                                         , asio::socket_base::message_flags flags
                                         , sys::error_code& ec)
{
    if (_debug) {
        log(this, " udp_multiplexer::send_to");
        for (auto b : buffers) {
            log(this, "    ", to_hex((uint8_t*)b.data(), b.size()));
        }
    }

    size_t sent = _udp_socket.send_to(buffers, destination, flags, ec);

    _send_to_signal(buffers, sent, destination, ec);

    return sent;
}

template< typename WriteHandler>
inline
void udp_multiplexer_impl::async_send_to( const std::vector<asio::const_buffer>& buffers
                                        , const endpoint_type& dst
                                        , WriteHandler&& h)
{
    _udp_socket.async_send_to(buffers, dst, [
        &buffers,
        &dst,
        h = std::forward<WriteHandler>(h),
        self = shared_from_this()
    ] (const sys::error_code& ec, std::size_t bytes_transferred) mutable {
        self->_send_to_signal(buffers, bytes_transferred, dst, ec);
        h(ec, bytes_transferred);
    });
}

inline
udp_multiplexer_impl::on_send_to_connection udp_multiplexer_impl::on_send_to(std::function<on_send_to_handler> handler)
{
    return _send_to_signal.connect(std::move(handler));
}

inline
size_t udp_multiplexer_impl::available(sys::error_code& ec) const
{
    return _udp_socket.available(ec);
}

inline
udp_multiplexer_impl::~udp_multiplexer_impl() {
    if (_debug) {
        log(this, " ~udp_multiplexer_impl");
    }

    auto& s = asio::use_service<service>(_udp_socket.get_executor().context());
    s.erase_multiplexer(local_endpoint());
}

inline
std::string udp_multiplexer_impl::to_hex(uint8_t* data, size_t size)
{
    std::stringstream ss;
    static const char chs[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        auto ch = data[i];
        ss << chs[(ch >> 4) & 0xf] << chs[ch & 0xf];
    }
    return ss.str();
}

} // asio_utp
