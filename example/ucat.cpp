#include <iostream>
#include <utp.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <unistd.h> // dup

#include "block.h"

#if BOOST_VERSION < 106400
#error "The ucat.cpp example requires Boost version 1.47 or higher"
// Because posix::stream_descriptor has trouble reading from STDIN_FILENO in
// earlier versions.
#endif

using namespace std;
namespace asio = boost::asio;
namespace ip   = asio::ip;
namespace sys  = boost::system;

struct defer {
    std::function<void()> f;
    ~defer() { f(); }
};

ip::udp::endpoint parse_endpoint(string s)
{
    auto pos = s.find(':');

    if (pos == string::npos) {
        throw runtime_error("Failed to parse endpoint");
    }

    auto addr = ip::address::from_string(s.substr(0, pos));
    auto port = s.substr(pos + 1);

    if (port.empty()) port = "0";

    return {addr, uint16_t(stoi(port))};
}

template<class S1, class S2>
void forward(S1& s1, S2& s2, asio::yield_context yield)
{
    std::vector<unsigned char> buffer(4*1024);

    sys::error_code ec;

    while (true) {
        size_t n = s1.async_read_some(asio::buffer(buffer), yield[ec]);
        if (ec) return;
        asio::async_write(s2, asio::buffer(buffer.data(), n), yield[ec]);
        if (ec) return;
    }
}

void forward(utp::socket s, asio::yield_context yield)
{
    auto& ios = s.get_io_service();

    block b1(ios), b2(ios);

    asio::posix::stream_descriptor output(ios, ::dup(STDOUT_FILENO));
    asio::posix::stream_descriptor input (ios, ::dup(STDIN_FILENO));

    auto close_everything = [&] {
        s.close();
        if (output.is_open()) output.close();
        if (input .is_open()) input .close();
    };

    asio::spawn(ios, [&] (asio::yield_context yield) {
        defer on_exit{[&] { close_everything(); b1.release(); }};
        forward(s, output, yield);
    });

    asio::spawn(ios, [&] (asio::yield_context yield) {
        defer on_exit{[&] { close_everything(); b2.release(); }};
        forward(input, s, yield);
    });

    b1.wait(yield);
    b2.wait(yield);
}

void server( asio::io_service& ios
           , int argc
           , const char** argv
           , asio::yield_context yield)
{
    assert(argc >= 3);

    utp::socket s(ios, parse_endpoint(argv[2]));

    cerr << "Accepting on: " << s.local_endpoint() << endl;
    s.async_accept(yield);
    cerr << "Accepted"  << endl;

    forward(move(s), yield);
}

void client( asio::io_service& ios
           , int argc
           , const char** argv
           , asio::yield_context yield)
{
    assert(argc >= 3);

    utp::socket s(ios, {ip::address_v4::loopback(), 0});

    auto remote_ep = parse_endpoint(argv[2]);

    cerr << "Connecting to: " << remote_ep << endl;
    s.async_connect(remote_ep, yield);
    cerr << "Connected" << endl;

    forward(move(s), yield);
}

void usage(const char* app_name)
{
    cerr << "Usage:\n"
         << "  " << app_name << " s <endpoint-to-accept-on>" << endl
         << "  " << app_name << " c <endpoint-to-connect-to>" << endl;
}

int main(int argc, const char** argv)
{
    asio::io_service ios;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (argv[1] == string("c")) {
        asio::spawn(ios, [&] (asio::yield_context yield) {
            client(ios, argc, argv, yield);
        });
    }
    else if (argv[1] == string("s")) {
        asio::spawn(ios, [&] (asio::yield_context yield) {
            server(ios, argc, argv, yield);
        });
    }
    else {
        usage(argv[0]);
        return 1;
    }

    try {
        ios.run();
    }
    catch(const std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }
}
