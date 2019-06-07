#define BOOST_TEST_MODULE comm
#include <boost/test/included/unit_test.hpp>

#include <util.hpp>
#include <iostream>
#include <thread>

#include <asio_utp.hpp>
#include <namespaces.hpp>
#include <boost/asio/spawn.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
namespace ip = asio::ip;
using udp = ip::udp;
using namespace std;
namespace utp = asio_utp;

BOOST_AUTO_TEST_SUITE(comm_tests)

static asio::mutable_buffers_1 buffer(std::string& s) {
    return asio::buffer(const_cast<char*>(s.data()), s.size());
}

BOOST_AUTO_TEST_CASE(comm_no_block)
{
    asio::io_context ioc;

    sys::error_code ec;
    utp::socket s(ioc);
    s.bind({ip::address_v4::loopback(), 0}, ec);

    BOOST_REQUIRE(!ec);

    // When there are no async actions waiting to be completed, this shouldn't
    // block.
    ioc.run();
}

BOOST_AUTO_TEST_CASE(comm_server_reads)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        client_s.bind({ip::address_v4::loopback(), 0}, ec2);

        BOOST_REQUIRE(!ec1);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    size_t end_count = 2;

    // TODO: We shouldn't need this
    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    string tx_msg = "test";

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(tx_msg.size(), '\0');
        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE_EQUAL(rx_msg, tx_msg);

        on_finish();
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        client_s.async_write_some(asio::buffer(tx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);

        on_finish();
    });

    ioc.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}


BOOST_AUTO_TEST_CASE(comm_exchange)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        client_s.bind({ip::address_v4::loopback(), 0}, ec2);

        BOOST_REQUIRE(!ec1);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ioc, [&](asio::yield_context yield) {
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

    asio::spawn(ioc, [&](asio::yield_context yield) {
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

    ioc.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}


BOOST_AUTO_TEST_CASE(comm_test2)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        client_s.bind({ip::address_v4::loopback(), 0}, ec2);

        BOOST_REQUIRE(!ec1);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ioc, [&](asio::yield_context yield) {
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

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        string tx_msg("aabbcc");
        client_s.async_write_some(asio::buffer(tx_msg), yield[ec]);
        BOOST_REQUIRE(!ec);

        on_finish();
    });

    ioc.run();

    BOOST_REQUIRE_EQUAL(end_count, size_t(0));
}


BOOST_AUTO_TEST_CASE(socket_local_random_bind)
{
    asio::io_context ioc;

    utp::socket s(ioc);

    sys::error_code ec;

    s.bind({ip::address_v4::loopback(), 0}, ec);

    BOOST_REQUIRE(!ec);

    BOOST_REQUIRE(s.local_endpoint().port());
}


BOOST_AUTO_TEST_CASE(comm_same_endpoint_multiplex)
{
    asio::io_context ioc;

    utp::socket server1(ioc);
    utp::socket server2(ioc);

    {
        sys::error_code ec1, ec2;

        server1.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        server2.bind(server1.local_endpoint(), ec2);
        BOOST_REQUIRE(!ec2);
    }

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        server1.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        server2.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);
        server1.close();
        server2.close();
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        utp::socket client1(ioc);
        utp::socket client2(ioc);

        {
            sys::error_code ec1, ec2;

            client1.bind({ip::address_v4::loopback(), 0}, ec1);
            BOOST_REQUIRE(!ec1);

            client2.bind(client1.local_endpoint(), ec2);
            BOOST_REQUIRE(!ec2);
        }

        client1.async_connect(server1.local_endpoint(), yield[ec]);
        BOOST_REQUIRE(!ec);

        client2.async_connect(server1.local_endpoint(), yield[ec]);
        BOOST_REQUIRE(!ec);
    });

    ioc.run();
}


// TODO: This test works but takes long time for the sockets to stop after
// successfully doing the large data send/receive.
BOOST_AUTO_TEST_CASE(comm_send_large_data)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        client_s.bind({ip::address_v4::loopback(), 0}, ec2);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    srand(time(nullptr));

    std::vector<uint8_t> data(1024);

    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = uint8_t(i % 256);
    }

    asio::spawn(ioc, [&](asio::yield_context yield) {
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
        //server_s.close();
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
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

    ioc.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_accept)
{
    asio::io_context ioc;

    utp::socket socket(ioc);

    {
        sys::error_code ec;
        socket.bind({ip::address_v4::loopback(), 0}, ec);
        BOOST_REQUIRE(!ec);
    }

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        asio::spawn(ioc, [&socket, &ioc] (asio::yield_context yield) {
            ioc.post(yield); // So that closing happens _after_ the accept
            socket.close();
        });

        socket.async_accept(yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);
    });

    ioc.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_connect)
{
    asio::io_context ioc;

    utp::socket client_s(ioc);
    utp::socket server_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        client_s.bind({ip::address_v4::loopback(), 0}, ec2);
        BOOST_REQUIRE(!ec2);
    }

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        asio::spawn(ioc, [&client_s, &ioc] (asio::yield_context yield) {
            ioc.post(yield); // So that closing happens _after_ the accept
            client_s.close();
        });

        client_s.async_connect(server_s.local_endpoint(), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        server_s.close();
    });

    ioc.run();
}


BOOST_AUTO_TEST_CASE(comm_abort_recv)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        client_s.bind({ip::address_v4::loopback(), 0}, ec2);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    size_t end_count = 2;

    auto on_finish = [&] {
        if (--end_count != 0) return;
        client_s.close();
        server_s.close();
    };

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        asio::spawn(ioc, [&server_s, &ioc](asio::yield_context yield) {
            ioc.post(yield);
            server_s.close();
        });

        string rx_msg(256, '\0');
        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        on_finish();
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        asio::spawn(ioc, [&client_s, &ioc](asio::yield_context yield) {
            ioc.post(yield);
            client_s.close();
        });

        string rx_msg(256, '\0');
        client_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::operation_aborted);

        on_finish();
    });

    ioc.run();

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
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        client_s.bind({ip::address_v4::loopback(), 0}, ec2);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string rx_msg(256, '\0');
        server_s.async_read_some(buffer(rx_msg), yield[ec]);
        BOOST_REQUIRE_EQUAL(ec, asio::error::connection_reset);
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        client_s.async_connect(server_ep, yield[ec]);
        BOOST_REQUIRE(!ec);

        client_s.close();
    });

    ioc.run();
}

BOOST_AUTO_TEST_CASE(comm_client_eof)
{
    asio::io_context ioc;

    utp::socket server_s(ioc);
    utp::socket client_s(ioc);

    {
        sys::error_code ec1, ec2;

        server_s.bind({ip::address_v4::loopback(), 0}, ec1);
        BOOST_REQUIRE(!ec1);

        client_s.bind({ip::address_v4::loopback(), 0}, ec2);
        BOOST_REQUIRE(!ec2);
    }

    auto server_ep = server_s.local_endpoint();

    asio::spawn(ioc, [&](asio::yield_context yield) {
        sys::error_code ec;

        server_s.async_accept(yield[ec]);
        BOOST_REQUIRE(!ec);

        string msg(256, '\0');
        server_s.async_read_some(buffer(msg), yield[ec]);

        server_s.close();
    });

    asio::spawn(ioc, [&](asio::yield_context yield) {
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

    ioc.run();
}

BOOST_AUTO_TEST_SUITE_END()
