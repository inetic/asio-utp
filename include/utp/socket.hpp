#pragma once

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/intrusive/list.hpp>

namespace utp {

class udp_loop;

class socket {
public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

private:
    using connect_handler_type
        = std::function<void(const boost::system::error_code&)>;

    using accept_handler_type
        = std::function<void(const boost::system::error_code&)>;

    using send_handler_type
        = std::function<void(const boost::system::error_code&, size_t)>;

    using recv_handler_type
        = std::function<void(const boost::system::error_code&, size_t)>;

private:
    using accept_hook_type
        = boost::intrusive::list_base_hook
              <boost::intrusive::link_mode
                  <boost::intrusive::auto_unlink>>;

public:
    socket(const socket&) = delete;
    socket& operator=(const socket&) = delete;

    socket(socket&&);
    socket& operator=(socket&&);

    socket(boost::asio::io_service&);

    void bind(const endpoint_type&);

    template<typename CompletionToken>
    void async_connect(const endpoint_type&, CompletionToken&&);

    template<typename CompletionToken>
    void async_accept(CompletionToken&&);

    template< typename ConstBufferSequence
            , typename CompletionToken>
    auto async_send(const ConstBufferSequence&, CompletionToken&&);

    template< typename MutableBufferSequence
            , typename CompletionToken>
    auto async_receive(const MutableBufferSequence&, CompletionToken&&);

    boost::asio::ip::udp::endpoint local_endpoint() const;

    void close();

    ~socket();

private:
    friend class ::utp::udp_loop;

    void on_connect();
    void on_writable();
    void on_eof();
    void on_destroy();
    void on_accept(void* usocket);
    void on_receive(const unsigned char*, size_t);

    socket(std::shared_ptr<udp_loop>, void* utp_socket);

    accept_hook_type _accept_hook;

    void do_send(send_handler_type&&);
    void do_receive(recv_handler_type&&);
    void do_connect(const endpoint_type&);
    void do_accept(accept_handler_type&&);

    void close_with_error(const boost::system::error_code&);

private:
    boost::asio::io_service& _ios;

    void* _utp_socket = nullptr;

    std::shared_ptr<udp_loop> _udp_loop;

    connect_handler_type _connect_handler;
    accept_handler_type  _accept_handler;
    send_handler_type    _send_handler;
    recv_handler_type    _recv_handler;

    size_t _bytes_sent = 0;
    std::vector<boost::asio::const_buffer> _tx_buffers;

    struct buf_t : public std::vector<unsigned char> {
        using std::vector<unsigned char>::vector;

        size_t consumed = 0;

        operator boost::asio::const_buffer() const {
            assert(consumed <= this->size());
            return boost::asio::const_buffer( this->data() + consumed
                                            , this->size());
        }
    };

    // TODO: std::queue is not iterable (required by BufferSequences).
    // Perhaps use something like this?
    // https://stackoverflow.com/a/5984198/273348
    std::vector<buf_t> _rx_buffer_queue;
    std::vector<boost::asio::mutable_buffer> _rx_buffers;
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
    _connect_handler = std::move(handler);

    do_connect(ep);

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

    do_accept(std::move(handler));

    return result.get();
}

template< typename ConstBufferSequence
        , typename CompletionToken>
inline
auto socket::async_send( const ConstBufferSequence& bufs
                       , CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    _tx_buffers.clear();
    std::copy(bufs.begin(), bufs.end(), std::back_inserter(_tx_buffers));

    using handler_type = typename asio::handler_type
                           < CompletionToken
                           , void(system::error_code, size_t)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);
    do_send(std::move(handler));

    return result.get();
}

template< typename MutableBufferSequence
        , typename CompletionToken>
inline
auto socket::async_receive( const MutableBufferSequence& bufs
                          , CompletionToken&& token)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    _rx_buffers.clear();
    std::copy(bufs.begin(), bufs.end(), std::back_inserter(_rx_buffers));

    using handler_type = typename asio::handler_type
                           < CompletionToken
                           , void(system::error_code, size_t)>::type;

    handler_type handler(std::forward<decltype(token)>(token));
    asio::async_result<handler_type> result(handler);
    do_receive(std::move(handler));

    return result.get();
}

} // namespace
