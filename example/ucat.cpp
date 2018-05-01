#include <iostream>
#include <utp.hpp>
#include <boost/asio.hpp>

using namespace std;
namespace asio = boost::asio;
namespace ip   = asio::ip;
namespace sys  = boost::system;

void be_server(asio::io_service& ios)
{
    utp::socket s(ios);

    ip::udp::endpoint local_ep(ip::address_v4::loopback(), 1234);
              
    s.bind(local_ep);

    ios.run();
}

void be_client(asio::io_service& ios)
{
    utp::socket s(ios);

    ip::udp::endpoint local_ep(ip::address_v4::loopback(), 0);
    ip::udp::endpoint remote_ep(ip::address_v4::loopback(), 1234);
              
    s.bind(local_ep);

    s.async_connect(remote_ep, [] (const sys::error_code& ec) {
            cout << "on connect: " << ec.message();
        });

    ios.run();
}

void usage(const char* app_name)
{
    cerr << "Usage:\n"
         << "  " << app_name << " {s,c}" << endl;
}

int main(int argc, const char** argv)
{
    asio::io_service ios;

    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    if (argv[1] == string("c")) {
        be_client(ios);
    }
    else if (argv[1] == string("s")) {
        be_server(ios);
    }
    else {
        usage(argv[0]);
        return 1;
    }
}
