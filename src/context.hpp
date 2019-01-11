#pragma once

#include <boost/asio/ip/udp.hpp>
#include <iostream>
#include <map>
#include "namespaces.hpp"
#include "util.hpp"
#include "socket_impl.hpp"

#include <utp.h>
#include <asio_utp/socket.hpp>

namespace asio_utp {

class context : public std::enable_shared_from_this<context> {
public:
    using endpoint_type = asio::ip::udp::endpoint;
    using socket_type = asio::ip::udp::socket;

public:
    context(socket_type socket);

    utp_context* get_libutp_context() const { return _utp_ctx; }

    const socket_type& udp_socket() const { return _socket; }

    asio::io_context::executor_type get_executor();

    bool socket_is_open() const { return _socket.is_open(); }

    ~context();

    static std::shared_ptr<context>
        get_or_create(asio::io_context&, const endpoint_type&);

private:
    friend class ::asio_utp::socket_impl;

    void increment_use_count();
    void decrement_use_count();

    void start();
    void stop();
    void start_reading();

    void on_read(const sys::error_code& ec, size_t size);

    static uint64 callback_log(utp_callback_arguments*);
    static uint64 callback_sendto(utp_callback_arguments*);
    static uint64 callback_on_error(utp_callback_arguments*);
    static uint64 callback_on_state_change(utp_callback_arguments*);
    static uint64 callback_on_read(utp_callback_arguments*);
    static uint64 callback_on_firewall(utp_callback_arguments*);
    static uint64 callback_on_accept(utp_callback_arguments*);

    static std::map<endpoint_type, std::shared_ptr<context>>& contexts();

private:
    socket_type _socket;
    utp_context* _utp_ctx;
    asio::ip::udp::endpoint _rx_endpoint;
    std::array<char, 4096> _rx_buffer;
    // Number of `socket_impl`ementations referencing using `this`.
    size_t _use_count = 0;

    boost::intrusive::list
        < socket_impl
        , boost::intrusive::member_hook< socket_impl
                                       , socket_impl::accept_hook_type
                                       , &socket_impl::_accept_hook
                                       >
        , boost::intrusive::constant_time_size<false>
        >
        _accepting_sockets;

    struct ticker_type;
    std::shared_ptr<ticker_type> _ticker;
};

} // namespace
