#pragma once

#include <boost/asio/ip/udp.hpp>
#include <asio_utp/detail/handler.hpp>
#include <asio_utp/detail/signal.hpp>

namespace asio_utp {

class udp_multiplexer_impl;
class socket_impl;

class udp_multiplexer {
private:
    struct state;

public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

    using on_send_to_handler = void(
        const std::vector<boost::asio::const_buffer>&,
        size_t,
        const endpoint_type&,
        boost::system::error_code
    );
    using on_send_to_connection = Signal<on_send_to_handler>::Connection;

public:
    udp_multiplexer() = default;

    udp_multiplexer(const udp_multiplexer&) = delete;
    udp_multiplexer& operator=(const udp_multiplexer&) = delete;

    udp_multiplexer(udp_multiplexer&&) = default;
    udp_multiplexer& operator=(udp_multiplexer&&) = default;

    udp_multiplexer(boost::asio::io_context&);
    udp_multiplexer(const boost::asio::executor&);

    void bind(const endpoint_type& local_endpoint, boost::system::error_code&);
    void bind(const udp_multiplexer&, boost::system::error_code&);

    template< typename MutableBufferSequence
            , typename CompletionToken>
    auto async_receive_from( const MutableBufferSequence&
                           , endpoint_type&
                           , CompletionToken&&);

    template< typename ConstBufferSequence
            , typename CompletionToken>
    auto async_send_to( const ConstBufferSequence&
                      , const endpoint_type& destination
                      , CompletionToken&&);

    on_send_to_connection on_send_to(std::function<on_send_to_handler> handler);

    boost::asio::executor get_executor()
    {
        return _ex;
    }

    endpoint_type local_endpoint() const;

    bool is_open() const;

    void close(boost::system::error_code&);

    ~udp_multiplexer();

private:
    void do_receive(endpoint_type& ep, handler<size_t>&&);
    void do_send(const endpoint_type& ep, handler<size_t>&&);

    std::vector<boost::asio::mutable_buffer>* rx_buffers();
    std::vector<boost::asio::const_buffer>*   tx_buffers();

    friend class socket_impl;
    std::shared_ptr<udp_multiplexer_impl> impl() const;

private:
    boost::asio::executor _ex;
    std::shared_ptr<state> _state;
};

template< typename MutableBufferSequence
        , typename CompletionToken>
inline
auto udp_multiplexer::async_receive_from( const MutableBufferSequence& bufs
                                        , endpoint_type& ep
                                        , CompletionToken&& token)
{
    if (auto rx_bufs = rx_buffers()) {
        rx_bufs->clear();

        std::copy( boost::asio::buffer_sequence_begin(bufs)
                 , boost::asio::buffer_sequence_end(bufs)
                 , std::back_inserter(*rx_bufs));
    }

    boost::asio::async_completion
        < CompletionToken
        , void(boost::system::error_code, size_t)
        > c(token);

    do_receive(ep, {get_executor(), std::move(c.completion_handler)});

    return c.result.get();
}

template< typename ConstBufferSequence
        , typename CompletionToken>
inline
auto udp_multiplexer::async_send_to( const ConstBufferSequence& bufs
                                   , const endpoint_type& destination
                                   , CompletionToken&& token)
{
    if (auto tx_bufs = tx_buffers()) {
        tx_bufs->clear();

        std::copy( boost::asio::buffer_sequence_begin(bufs)
                 , boost::asio::buffer_sequence_end(bufs)
                 , std::back_inserter(*tx_bufs));
    }

    boost::asio::async_completion
        < CompletionToken
        , void(boost::system::error_code, size_t)
        > c(token);

    do_send(destination, {get_executor(), std::move(c.completion_handler)});

    return c.result.get();
}

} // asio_utp
