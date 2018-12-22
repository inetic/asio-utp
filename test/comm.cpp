#define BOOST_TEST_MODULE comm
#include <boost/test/included/unit_test.hpp>

#include <util.hpp>
#include <iostream>
#include <thread>

#include <utp.hpp>
#include <namespaces.hpp>
#include <boost/asio/spawn.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
namespace ip = asio::ip;
using udp = ip::udp;
using namespace std;

BOOST_AUTO_TEST_SUITE(comm_tests)

static asio::mutable_buffers_1 buffer(std::string& s) {
    return asio::buffer(const_cast<char*>(s.data()), s.size());
}


BOOST_AUTO_TEST_CASE(comm_test)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(256, '\0');
        size_t size = server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE_EQUAL(rx_msg.substr(0, size), "hello from client");

        string tx_msg("hello from server");

        server_s.async_write_some(asio::buffer(tx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);

        on_finish();
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        string tx_msg("hello from client");
        client_s.async_write_some(asio::buffer(tx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(256, '\0');
        size_t size = client_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE_EQUAL(rx_msg.substr(0, size), "hello from server");

        on_finish();
    });

    ios.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}


BOOST_AUTO_TEST_CASE(comm_test2)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        vector<string> expect({"aa", "bb", "cc"});

        for (auto e : expect) {
            string rx_msg(e.size(), '\0');
            size_t size = server_s.async_read_some(buffer(rx_msg), yield[ec]);
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE_EQUAL(rx_msg.substr(0, size), e);
        }

        on_finish();
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        string tx_msg("aabbcc");
        client_s.async_write_some(asio::buffer(tx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);

        on_finish();
    });

    ios.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}

// TODO: This test works but takes long time for the sockets to stop after
// successfully doing the large data send/receive.
BOOST_AUTO_TEST_CASE(comm_send_large_data)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    srand(time(nullptr));

    std::vector<uint8_t> data(1024);

    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = uint8_t(i % 256);
    }

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(256, '\0');

        size_t d = 0;
        while (true) {
            size_t n = server_s.async_read_some(buffer(rx_msg), yield[ec]);
            for (size_t i = 0; i < n; ++i) {
                BOOST_REQUIRE_EQUAL(uint8_t(rx_msg[i]), uint8_t(d++ % 256));
            }
            BOOST_REQUIRE_EQUAL(ec, sys::error_code());
            if (d == data.size()) break;
        }

        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::connection_reset);
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        asio::const_buffer buf(data.data(), data.size());

        size_t sent = 0;
        while (sent != data.size()) {
            size_t k = std::min<size_t>(data.size() - sent, 333);

            size_t n = client_s.async_write_some
                ( asio::buffer(data.data() + sent, k)
                , yield[ec]);

            BOOST_REQUIRE(!ec);
            sent += n;
        }

        client_s.close();
    });

    ios.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_accept)
{
    asio::io_service ios;

    utp::socket socket(ios);

    socket.bind({ip::address_v4::loopback(), 0});

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        asio::spawn(ios, [&socket, &ios] (asio::yield_context yield) {
            ios.post(yield); // So that closing happens _after_ the accept
            socket.close();
        });

        socket.async_accept(yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);
    });

    ios.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_connect)
{
    asio::io_service ios;

    utp::socket client_s(ios);
    utp::socket server_s(ios);

    client_s.bind({ip::address_v4::loopback(), 0});
    server_s.bind({ip::address_v4::loopback(), 0});

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        asio::spawn(ios, [&client_s, &ios] (asio::yield_context yield) {
            ios.post(yield); // So that closing happens _after_ the accept
            client_s.close();
        });

        client_s.async_connect(server_s.local_endpoint(), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        server_s.close();
    });

    ios.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_recv)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        asio::spawn(ios, [&server_s, &ios](asio::yield_context yield) {
            ios.post(yield);
            server_s.close();
        });

        string rx_msg(256, '\0');
        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        on_finish();
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        asio::spawn(ios, [&client_s, &ios](asio::yield_context yield) {
            ios.post(yield);
            client_s.close();
        });

        string rx_msg(256, '\0');
        client_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        on_finish();
    });

    ios.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}

// The next step could be made faster with the following patch:
//
// diff --git a/utp_internal.cpp b/utp_internal.cpp
// index ec6bb04..dcbf305 100644
// --- a/utp_internal.cpp
// +++ b/utp_internal.cpp
// @@ -2315,7 +2315,8 @@ size_t utp_process_incoming(UTPSocket *conn, const byte *packet, size_t len, boo
//         // The connection is not in a state that can accept data?
//         if (conn->state != CS_CONNECTED &&
//                 conn->state != CS_CONNECTED_FULL &&
// -               conn->state != CS_FIN_SENT) {
// +               conn->state != CS_FIN_SENT &&
// +               conn->state != CS_SYN_RECV) {
//                 return 0;
//         }
//
// It allows the socket which is in the CS_SYN_RECV state to receive a FIN
// packet and call the state change handler.
//
BOOST_AUTO_TEST_CASE(comm_server_eof)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(256, '\0');
        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::connection_reset);
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        client_s.close();
    });

    ios.run();
}

BOOST_AUTO_TEST_CASE(comm_client_eof)
{
    asio::io_service ios;

    utp::socket server_s(ios);
    server_s.bind({ip::address_v4::loopback(), 0});
    auto server_ep = server_s.local_endpoint();

    utp::socket client_s(ios);
    client_s.bind({ip::address_v4::loopback(), 0});

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string msg(256, '\0');
        server_s.async_read_some(buffer(msg), yield[ec]);

        server_s.close();
    });

    asio::spawn(ios, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        // We must write to the server first, otherwise it'll ignore us
        // completely.
        string msg(256, '\0');
        client_s.async_write_some(buffer(msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, sys::error_code());

        client_s.async_read_some(buffer(msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::connection_reset);
    });

    ios.run();
}

BOOST_AUTO_TEST_SUITE_END()
