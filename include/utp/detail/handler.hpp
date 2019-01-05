#pragma once

#include <vector>
#include <iostream>

namespace utp {

template<typename... Args>
class handler {
private:
    using error_code = boost::system::error_code;

    struct base {
        virtual void operator()(const error_code&, Args...) = 0;
        virtual ~base() {};
    };

    template<class Executor, class Func>
    struct impl final : public base {
        Executor e;
        Func f;
        boost::asio::executor_work_guard<Executor> w;

        template<class E, class F>
        impl(E&& e, F&& f)
            : e(std::forward<E>(e))
            , f(std::forward<F>(f))
            , w(this->e)
        {}

        void operator()(const error_code& ec, Args... args) override
        {
            f(ec, args...);
        }
    };

public:
    handler() = default;
    handler(const handler&) = default;

    handler(handler&& h) : _impl(std::move(h._impl)) { }

    handler& operator=(handler&& h)
    {
        _impl = std::move(h._impl);
        return *this;
    }

    template<class Executor, class Func> handler(Executor&& exec, Func&& func)
    {
        auto e = boost::asio::get_associated_executor(func, exec);

        _impl.reset(new impl<decltype(e), Func>( std::move(e)
                                               , std::forward<Func>(func)));
    }

    void operator()(const error_code& ec, Args... args)
    {
        (*_impl)(ec, args...);
    }

    void operator()(const error_code& ec, Args... args) const
    {
        (*_impl)(ec, args...);
    }

    operator bool() const { return bool(_impl); }

private:
    std::shared_ptr<base> _impl;
};

} // namespace
