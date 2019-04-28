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
    srand(time(nullptr));

    auto r8 = [] { return (uint8_t) (rand() % 256); };
    auto r16 = [] { return (uint8_t) (rand() % 256); };

    {
        ip::address_v4 addr({r8(), r8(), r8(), r8()});
        udp::endpoint ep(addr, r16());

        BOOST_CHECK_EQUAL(ep, util::to_endpoint_v4(util::to_sockaddr_v4(ep)));
        BOOST_CHECK_EQUAL(ep, util::to_endpoint(util::to_sockaddr(ep)));
    }

    {
        ip::address_v6 addr({ r8(), r8(), r8(), r8(), r8(), r8(), r8(), r8()
                            , r8(), r8(), r8(), r8(), r8(), r8(), r8(), r8()});

        udp::endpoint ep(addr, r16());

        BOOST_CHECK_EQUAL(ep, util::to_endpoint_v6(util::to_sockaddr_v6(ep)));
        BOOST_CHECK_EQUAL(ep, util::to_endpoint(util::to_sockaddr(ep)));
    }
}

BOOST_AUTO_TEST_SUITE_END()
