#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/intrusive/list.hpp>

#include <utp/impl/socket_impl.hpp>

namespace utp {

class socket {
public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

public:
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    socket(socket&&) = default;
    socket& operator=(socket&&) = default;

    socket(boost::asio::io_service&);

    void bind(const endpoint_type&);

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

    boost::asio::ip::udp::endpoint local_endpoint() const;

    void close();

    boost::asio::io_service& get_io_service() const { return *_ios; }

    ~socket();

private:
    boost::asio::io_service* _ios = nullptr;
    std::shared_ptr<socket_impl> _socket_impl;
};

template<typename CompletionToken>
inline
void socket::async_connect(const endpoint_type& ep, CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    using handler_type = typename asio::handler_type
                           < CompletionToken, void(system::error_code)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);

    _socket_impl->do_connect(ep, std::move(handler));

    return result.get();
}

template<typename CompletionToken>
void socket::async_accept(CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    using handler_type = typename asio::handler_type
                           < CompletionToken, void(system::error_code)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);

    _socket_impl->do_accept(std::move(handler));

    return result.get();
}

template< typename ConstBufferSequence
        , typename CompletionToken>
inline
auto socket::async_write_some( const ConstBufferSequence& bufs
                             , CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    _socket_impl->_tx_buffers.clear();

    std::copy( bufs.begin()
             , bufs.end()
             , std::back_inserter(_socket_impl->_tx_buffers));

    using handler_type = typename asio::handler_type
                           < CompletionToken
                           , void(system::error_code, size_t)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);

    _socket_impl->do_send(std::move(handler));

    return result.get();
}

template< typename MutableBufferSequence
        , typename CompletionToken>
inline
auto socket::async_read_some( const MutableBufferSequence& bufs
                            , CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    _socket_impl->_rx_buffers.clear();

    std::copy( bufs.begin()
             , bufs.end()
             , std::back_inserter(_socket_impl->_rx_buffers));

    using handler_type = typename asio::handler_type
                           < CompletionToken
                           , void(system::error_code, size_t)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);

    _socket_impl->do_receive(std::move(handler));

    return result.get();
}

} // namespace
