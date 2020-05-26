#pragma once

#include <boost/asio/ip/udp.hpp>
#include <iostream>
#include <map>
#include "namespaces.hpp"
#include "util.hpp"
#include "socket_impl.hpp"
#include "udp_multiplexer_impl.hpp"
#include "intrusive_list.hpp"

#include <utp.h>
#include <asio_utp/socket.hpp>

namespace asio_utp {


class context : public std::enable_shared_from_this<context> {
public:
    using endpoint_type = asio::ip::udp::endpoint;
    using socket_type = asio::ip::udp::socket;
    using executor_type = socket_type::executor_type;

public:
    context(std::shared_ptr<udp_multiplexer_impl>);

    utp_context* get_libutp_context() const { return _utp_ctx; }

    endpoint_type local_endpoint() const { return _local_endpoint; }

    executor_type get_executor();

    ~context();

    static std::shared_ptr<context>
        get_or_create(asio::io_context&, const endpoint_type&);

    void increment_outstanding_ops(const char* dbg);
    void decrement_outstanding_ops(const char* dbg);
    void increment_completed_ops(const char* dbg);
    void decrement_completed_ops(const char* dbg);

private:
    static void erase_context(endpoint_type);
    void start_receiving();

private:
    friend class ::asio_utp::socket_impl;

    void register_socket(socket_impl&);
    void unregister_socket(socket_impl&);

    void start();
    void stop();
    void start_reading();

    void on_read( const sys::error_code& ec
                , const endpoint_type& ep
                , const std::vector<uint8_t>& data);

    static uint64 callback_log(utp_callback_arguments*);
    static uint64 callback_sendto(utp_callback_arguments*);
    static uint64 callback_on_error(utp_callback_arguments*);
    static uint64 callback_on_state_change(utp_callback_arguments*);
    static uint64 callback_on_read(utp_callback_arguments*);
    static uint64 callback_on_firewall(utp_callback_arguments*);
    static uint64 callback_on_accept(utp_callback_arguments*);

    static std::map<endpoint_type, std::weak_ptr<context>>& contexts();

private:
    std::shared_ptr<udp_multiplexer_impl> _multiplexer;
    udp_multiplexer_impl::recv_entry _recv_handle;
    endpoint_type _local_endpoint;
    utp_context* _utp_ctx;

    // Registered sockets are all those that use `this`.
    intrusive::list<socket_impl, &socket_impl::_register_hook> _registered_sockets;
    intrusive::list<socket_impl, &socket_impl::_accept_hook> _accepting_sockets;

    struct ticker_type;
    std::shared_ptr<ticker_type> _ticker;

    // Number of operation started but their handler have
    // not yet been put onto the execution queue.
    size_t _outstanding_op_count = 0;
    // Number of operations waiting on the execution queue.
    size_t _completed_op_count = 0;

#if ASIO_UTP_DEBUG_LOGGING
    bool _debug = true;
#else
    bool _debug = false;
#endif
};

} // namespace
