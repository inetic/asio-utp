#pragma once

#include <memory>
#include <boost/asio/spawn.hpp>

class block {
public:
    block(boost::asio::io_service& ios);
    block(const block&) = delete;
    block& operator=(const block&) = delete;

    ~block();

    void release();
    void wait(boost::asio::yield_context yield);

private:
    boost::asio::io_service& _ios;
    std::function<void(boost::system::error_code)> _on_notify;
    bool _released = false;
};

inline
block::block(boost::asio::io_service& ios)
    : _ios(ios)
{}

inline
block::~block()
{
    if (!_on_notify) return;

    _ios.post([h = std::move(_on_notify)] {
            h(boost::asio::error::operation_aborted);
        });
}

inline
void block::release()
{
    _released = true;

    if (!_on_notify) return;

    _ios.post([h = std::move(_on_notify)] {
            h(boost::system::error_code());
        });
}

inline
void block::wait(boost::asio::yield_context yield)
{
    namespace asio   = boost::asio;
    namespace system = boost::system;

    if (_released) return;

    asio::async_completion<decltype(yield), void(system::error_code)> c(yield);

    _on_notify = [ h = std::move(c.completion_handler)
                 , w = asio::io_service::work(_ios)
                 ] (const system::error_code& ec) mutable {
                     h(ec);
                 };

    return c.result.get();
}
