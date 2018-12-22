#pragma once

#include <boost/asio/ip/udp.hpp>
#include <iostream>
#include "namespaces.hpp"
#include "util.hpp"

#include <utp.h>
#include <utp/socket.hpp>

namespace utp {

class udp_loop : public std::enable_shared_from_this<udp_loop> {
public:
    udp_loop(asio::ip::udp::socket socket);

    void start();
    void stop();

    utp_context* get_utp_context() const { return _utp_ctx; }

    const asio::ip::udp::socket& udp_socket() const { return _socket; }

    asio::io_service& get_io_service();

    bool socket_is_open() const { return _socket.is_open(); }

private:
    friend class ::utp::socket_impl;

    void start_reading();

    void on_read(const sys::error_code& ec, size_t size);

    static uint64 callback_log(utp_callback_arguments*);
    static uint64 callback_sendto(utp_callback_arguments*);
    static uint64 callback_on_error(utp_callback_arguments*);
    static uint64 callback_on_state_change(utp_callback_arguments*);
    static uint64 callback_on_read(utp_callback_arguments*);
    static uint64 callback_on_firewall(utp_callback_arguments*);
    static uint64 callback_on_accept(utp_callback_arguments*);

private:
    asio::ip::udp::socket _socket;
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
