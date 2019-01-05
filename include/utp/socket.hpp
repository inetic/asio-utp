#pragma once

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include "detail/handler.hpp"

namespace utp {

class socket_impl;

class socket {
public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

public:
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    socket(socket&&) = default;
    socket& operator=(socket&&) = default;

    socket(boost::asio::io_context&, const endpoint_type&);

    template<typename CompletionToken>
    void async_connect(const endpoint_type&, CompletionToken&&);

    template<typename CompletionToken>
    void async_accept(CompletionToken&&);

    template< typename ConstBufferSequence
            , typename CompletionToken>
    auto async_write_some(const ConstBufferSequence&, CompletionToken&&);

    template< typename MutableBufferSequence
            , typename CompletionToken>
    auto async_read_some(const MutableBufferSequence&, CompletionToken&&);

    endpoint_type local_endpoint() const;

    bool is_open() const;

    void close();

    boost::asio::io_context::executor_type get_executor() const
    {
        return _ioc->get_executor();
    }

    // For debugging only
    void* pimpl() const { return _socket_impl.get(); }

    ~socket();

private:
    typedef void(connect_signature)(boost::system::error_code);
    typedef void(accept_signature)(boost::system::error_code);

    void do_connect(const endpoint_type&, std::function<connect_signature>);
    void do_accept (std::function<accept_signature>);
    void do_write  (handler<size_t>&&);
    void do_read   (handler<size_t>&&);

    std::vector<boost::asio::const_buffer>& tx_buffers();
    std::vector<boost::asio::mutable_buffer>& rx_buffers();

private:
    boost::asio::io_context* _ioc = nullptr;
    std::shared_ptr<socket_impl> _socket_impl;
};

template<typename CompletionToken>
inline
void socket::async_connect(const endpoint_type& ep, CompletionToken&& token)
{
    boost::asio::async_completion<CompletionToken, connect_signature> c(token);
    do_connect(ep, std::move(c.completion_handler));
    return c.result.get();
}

template<typename CompletionToken>
inline
void socket::async_accept(CompletionToken&& token)
{
    boost::asio::async_completion<CompletionToken, accept_signature> c(token);
    do_accept(std::move(c.completion_handler));
    return c.result.get();
}

template< typename ConstBufferSequence
        , typename CompletionToken>
inline
auto socket::async_write_some( const ConstBufferSequence& bufs
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

    do_write(handler<size_t>(get_executor(), std::move(c.completion_handler)));

    return c.result.get();
}

template< typename MutableBufferSequence
        , typename CompletionToken>
inline
auto socket::async_read_some( const MutableBufferSequence& bufs
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

    do_read(handler<size_t>(get_executor(), std::move(c.completion_handler)));

    return c.result.get();
}

} // namespace
