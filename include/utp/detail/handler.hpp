#pragma once

#include <vector>
#include <iostream>

namespace utp {

class handler {
private:
    using error_code = boost::system::error_code;

    typedef void(*exec_type)(void*, const error_code&, size_t);
    typedef void(*destruct_type)(void*);

    template<class Func>
    struct operation {
        static void exec(void* data, const error_code& ec, size_t size)
        {
            (*reinterpret_cast<Func*>(data))(ec, size);
        }

        static void destruct(void* data)
        {
            reinterpret_cast<Func*>(data)->~Func();
        }
    };

public:
    handler() = default;

    handler(const handler&) = delete;

    handler(handler&& h)
        : _exec(h._exec)
        , _destruct(h._destruct)
        , _func_data(std::move(h._func_data))
    {
        h._exec = nullptr;
        h._destruct = nullptr;
    }

    template<class Func> handler(Func&& func)
    {
        _exec = operation<Func>::exec;
        _destruct = operation<Func>::destruct;

        _func_data.resize(sizeof(Func));
        new (_func_data.data()) Func(std::forward<Func>(func));
    }

    template<class Func> void operator=(Func&& func)
    {
        if (_func_data.size()) {
            _destruct(_func_data.data());
        }

        _exec = operation<Func>::exec;
        _destruct = operation<Func>::destruct;

        _func_data.resize(sizeof(Func));
        new (_func_data.data()) Func(std::forward<Func>(func));
    }

    void operator()(const error_code& ec, size_t size)
    {
        _exec(_func_data.data(), ec, size);
    }

    void operator()(const error_code& ec, size_t size) const
    {
        _exec((void*) _func_data.data(), ec, size);
    }

    operator bool() const {
        return _func_data.size();
    }

    ~handler()
    {
        if (_func_data.size()) {
            _destruct(_func_data.data());
        }
    }

private:
    exec_type _exec = nullptr;
    destruct_type _destruct = nullptr;
    std::vector<uint8_t> _func_data;
};

} // namespace
