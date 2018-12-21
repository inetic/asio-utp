#pragma once


namespace utp {
    
class udp_loop;
class socket;

class socket_impl {
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
    socket_impl(const socket_impl&) = delete;
    socket_impl& operator=(const socket_impl&) = delete;

    socket_impl(socket_impl&&) = delete;
    socket_impl& operator=(socket_impl&&) = delete;

    socket_impl(boost::asio::io_service&);

    void bind(const endpoint_type&);

    boost::asio::ip::udp::endpoint local_endpoint() const;

    void close();

    boost::asio::io_service& get_io_service() const { return _ios; }

    ~socket_impl();

private:
    friend class ::utp::udp_loop;
    friend class ::utp::socket;

    void on_connect();
    void on_writable();
    void on_eof();
    void on_destroy();
    void on_accept(void* usocket);
    void on_receive(const unsigned char*, size_t);

    socket_impl(std::shared_ptr<udp_loop>, void* utp_socket);

    accept_hook_type _accept_hook;

    void do_send(send_handler_type);
    void do_receive(recv_handler_type&&);
    void do_connect(const endpoint_type&, connect_handler_type&&);
    void do_accept(accept_handler_type&&);

    void close_with_error(const boost::system::error_code&);

private:
    boost::asio::io_service& _ios;

    void* _utp_socket = nullptr;
    bool _closed = false;

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
                                            , this->size() - consumed);
        }
    };

    // TODO: std::queue is not iterable (required by BufferSequences).
    // Perhaps use something like this?
    // https://stackoverflow.com/a/5984198/273348
    std::vector<buf_t> _rx_buffer_queue;
    std::vector<boost::asio::mutable_buffer> _rx_buffers;
};

} // namespace
