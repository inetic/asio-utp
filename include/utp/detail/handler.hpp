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
        virtual void post_exec(const error_code&, Args...) = 0;
        virtual ~base() {};
    };

    template<class Executor, class Allocator, class Func>
    struct impl final : public base {
        Executor e;
        Allocator a;
        Func f;
        boost::asio::executor_work_guard<Executor> w;

        template<class E, class A, class F>
        impl(E&& e, A&& a, F&& f)
            : e(std::forward<E>(e))
            , a(std::forward<A>(a))
            , f(std::forward<F>(f))
            , w(this->e)
        {}

        void operator()(const error_code& ec, Args... args) override
        {
            f(ec, args...);
        }

        void post_exec(const error_code& ec, Args... args) override
        {
            e.post(std::bind(std::move(f), ec, args...), a);
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
        namespace net = boost::asio;

        auto e = net::get_associated_executor(func, exec);

        auto a = net::get_associated_allocator
                   ( func
                   , std::allocator<void>());

        using impl_t = impl<decltype(e), decltype(a), Func>;

        // XXX: allocate `impl` using `a`
        _impl = std::make_shared<impl_t>( std::move(e)
                                        , std::move(a)
                                        , std::forward<Func>(func));
    }

    void operator()(const error_code& ec, Args... args)
    {
        (*_impl)(ec, args...);
    }

    void operator()(const error_code& ec, Args... args) const
    {
        (*_impl)(ec, args...);
    }

    void post_exec(const error_code& ec, Args... args) {
        _impl->post_exec(ec, args...);
    }

    operator bool() const { return bool(_impl); }

private:
    std::shared_ptr<base> _impl;
};

} // namespace
