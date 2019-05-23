#pragma once

#include <boost/asio/ip/udp.hpp>
#include <asio_utp/detail/handler.hpp>

namespace asio_utp {

class udp_multiplexer_impl;

class udp_multiplexer {
private:
    struct state;

public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

public:
    udp_multiplexer() = default;

    udp_multiplexer(const udp_multiplexer&) = delete;
    udp_multiplexer& operator=(const udp_multiplexer&) = delete;

    udp_multiplexer(udp_multiplexer&&) = default;
    udp_multiplexer& operator=(udp_multiplexer&&) = default;

    udp_multiplexer(boost::asio::io_context&, const endpoint_type&);

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

    boost::asio::io_context::executor_type get_executor() const
    {
        return _ioc->get_executor();
    }

    boost::asio::io_context& get_io_service() const
    {
        return _ioc->get_executor().context();
    }

    endpoint_type local_endpoint() const;

private:
    void do_receive(endpoint_type& ep, handler<size_t>&&);
    void do_send(const endpoint_type& ep, handler<size_t>&&);

    std::vector<boost::asio::mutable_buffer>& rx_buffers();
    std::vector<boost::asio::const_buffer>&   tx_buffers();

private:
    boost::asio::io_context* _ioc = nullptr;
    std::shared_ptr<state> _state;
};

template< typename MutableBufferSequence
        , typename CompletionToken>
inline
auto udp_multiplexer::async_receive_from( const MutableBufferSequence& bufs
                                        , endpoint_type& ep
                                        , CompletionToken&& token)
{
    rx_buffers().clear();

    std::copy( boost::asio::buffer_sequence_begin(bufs)
             , boost::asio::buffer_sequence_end(bufs)
             , std::back_inserter(rx_buffers()));

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
    tx_buffers().clear();

    std::copy( boost::asio::buffer_sequence_begin(bufs)
             , boost::asio::buffer_sequence_end(bufs)
             , std::back_inserter(tx_buffers()));

    boost::asio::async_completion
        < CompletionToken
        , void(boost::system::error_code, size_t)
        > c(token);

    do_send(destination, {get_executor(), std::move(c.completion_handler)});

    return c.result.get();
}

} // asio_utp
