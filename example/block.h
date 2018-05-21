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
    if (_released) return;

    using Handler = boost::asio::handler_type
        < boost::asio::yield_context
        , void(boost::system::error_code)>::type;

    Handler handler(yield);
    boost::asio::async_result<Handler> result(handler);

    _on_notify = std::move(
        [ h = std::move(handler)
        , w = boost::asio::io_service::work(_ios)
        ] (const boost::system::error_code& ec) mutable {
            h(ec);
        });

    return result.get();
}
