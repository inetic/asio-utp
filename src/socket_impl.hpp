#pragma once

#include <boost/intrusive/list.hpp>
#include "utp/detail/handler.hpp"

namespace utp {
    
class context;
class socket;

class socket_impl : public std::enable_shared_from_this<socket_impl> {
public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

private:
    using connect_handler_type
        = std::function<void(const boost::system::error_code&)>;

    using accept_handler_type
        = std::function<void(const boost::system::error_code&)>;

private:
    using accept_hook_type
        = boost::intrusive::list_base_hook
              <boost::intrusive::link_mode
                  <boost::intrusive::auto_unlink>>;

public:
    socket_impl(const socket_impl&) = delete;
    socket_impl& operator=(const socket_impl&) = delete;

    socket_impl(socket_impl&&) = delete;
    socket_impl& operator=(socket_impl&&) = delete;

    socket_impl(boost::asio::io_context&);

    void bind(const endpoint_type&);

    endpoint_type local_endpoint() const;

    void close();

    bool is_open() const { return _context && !_closed; }

    boost::asio::io_context::executor_type get_executor() const
    {
        return _ioc.get_executor();
    }

    ~socket_impl();

private:
    friend class ::utp::context;
    friend class ::utp::socket;

    void on_connect();
    void on_writable();
    void on_eof();
    void on_destroy();
    void on_accept(void* usocket);
    void on_receive(const unsigned char*, size_t);

    socket_impl(std::shared_ptr<context>, void* utp_socket);

    accept_hook_type _accept_hook;

    void do_write(handler<size_t>&&);
    void do_read(handler<size_t>&&);
    void do_connect(const endpoint_type&, connect_handler_type&&);
    void do_accept(accept_handler_type&&);

    void close_with_error(const boost::system::error_code&);

private:
    boost::asio::io_context& _ioc;

    void* _utp_socket = nullptr;
    bool _closed = false;

    std::shared_ptr<context> _context;

    connect_handler_type _connect_handler;
    accept_handler_type  _accept_handler;
    handler<size_t> _send_handler;
    handler<size_t> _recv_handler;

    size_t _bytes_sent = 0;
    std::vector<boost::asio::const_buffer> _tx_buffers;

    struct buf_t : public std::vector<unsigned char> {
        using std::vector<unsigned char>::vector;

        size_t consumed = 0;

        operator boost::asio::const_buffer() const {
            assert(consumed <= this->size());
            return boost::asio::const_buffer( this->data() + consumed
                                            , this->size() - consumed);
        }
    };

    // TODO: std::queue is not iterable (required by BufferSequences).
    // Perhaps use something like this?
    // https://stackoverflow.com/a/5984198/273348
    std::vector<buf_t> _rx_buffer_queue;
    std::vector<boost::asio::mutable_buffer> _rx_buffers;

    // This prevents `this` from being destroyed after `socket` is destroyed
    // until libutp destroys `this->_utp_socket` (there is some IO that is done
    // in the mean time, like sending FIN packets and such).
    std::shared_ptr<socket_impl> _self;
};

} // namespace
