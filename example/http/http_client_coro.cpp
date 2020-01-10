//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP client, coroutine
//
//------------------------------------------------------------------------------

#include <boost/range.hpp>
#include <boost/asio/error.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <asio_utp.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace utp = asio_utp;
using udp = boost::asio::ip::udp;
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
void
do_session(
    udp::endpoint const& endpoint,
    std::string const& host,
    std::string const& target,
    int version,
    net::io_context& ioc,
    net::yield_context yield)
{
    beast::error_code ec;

    utp::socket socket{ioc};

    socket.bind(udp::endpoint{net::ip::address_v4::any(), 0}, ec);

    if(ec)
        return fail(ec, "bind");

    // Make the connection on the IP address we get from a lookup
    socket.async_connect(endpoint, yield[ec]);
    if(ec)
        return fail(ec, "connect");

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Send the HTTP request to the remote host
    http::async_write(socket, req, yield[ec]);
    if(ec)
        return fail(ec, "write");

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    http::async_read(socket, b, res, yield[ec]);
    if(ec)
        return fail(ec, "read");

    // Write the message to standard out
    std::cout << res << std::endl;

    // Gracefully close the socket
    socket.close();

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if(ec && ec != beast::errc::not_connected)
        return fail(ec, "shutdown");

    // If we get here then the connection is closed gracefully
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 5 && argc != 6)
    {
        std::cerr <<
            "Usage: http-client-coro <ip> <port> <host> <target> [<HTTP version: 1.0 or 1.1(default)>]\n" <<
            "Example:\n" <<
            "    http-client-coro 127.0.0.1 80 /\n" <<
            "    http-client-coro 127.0.0.1 80 / 1.0\n";
        return EXIT_FAILURE;
    }
    auto const ip = argv[1];
    uint16_t const port = std::stoi(argv[2]);
    auto const host = argv[3];
    auto const target = argv[4];
    int version = argc == 6 && !std::strcmp("1.0", argv[5]) ? 10 : 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // Launch the asynchronous operation
    net::spawn(ioc, std::bind(
        &do_session,
        udp::endpoint{net::ip::address::from_string(ip), port},
        std::string(host),
        std::string(target),
        version,
        std::ref(ioc),
        std::placeholders::_1));

    // Run the I/O service. The call will return when
    // the get operation is complete.
    ioc.run();

    return EXIT_SUCCESS;
}
