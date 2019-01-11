#define BOOST_TEST_MODULE util
#include <boost/test/included/unit_test.hpp>

#include <util.hpp>
#include <iostream>

using namespace asio_utp;
namespace ip = boost::asio::ip;
using udp = boost::asio::ip::udp;
using namespace std;

BOOST_AUTO_TEST_SUITE(util_tests)

BOOST_AUTO_TEST_CASE(sockaddr_conversion)
{
    ip::address_v4 addr({192, 168, 0, 1});
    udp::endpoint ep(addr, 1234);

    BOOST_CHECK_EQUAL(ep, util::to_endpoint(util::to_sockaddr(ep)));
}

BOOST_AUTO_TEST_SUITE_END()
