#pragma once

#include <vector>
#include <iostream>

namespace utp {

template<typename... Args>
class handler {
private:
    using error_code = boost::system::error_code;

    typedef void(*exec_type)(void*, const error_code&, Args...);
    typedef void(*destruct_type)(void*);

    struct base {
        virtual void exec(const error_code&, Args...) = 0;
        virtual ~base() {};
    };

    template<class Func>
    struct impl final : public base {
        Func f;

        template<class F>
        impl(F&& f) : f(std::forward<F>(f)) {}

        void exec(const error_code& ec, Args... args) override
        {
            f(ec, args...);
        }
    };

public:
    handler() = default;

    handler(const handler&) = delete;

    handler(handler&& h) : _impl(std::move(h._impl)) { }

    template<class Func> handler(Func&& func)
    {
        _impl.reset(new impl<Func>(std::forward<Func>(func)));
    }

    void operator()(const error_code& ec, Args... args)
    {
        _impl->exec(ec, args...);
    }

    void operator()(const error_code& ec, Args... args) const
    {
        _impl->exec(ec, args...);
    }

    operator bool() const { return _impl; }

private:
    std::unique_ptr<base> _impl;
};

} // namespace
