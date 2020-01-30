#pragma once

#include <functional>
#include <iostream>

#include <boost/intrusive/list.hpp>

namespace asio_utp {

template<typename T>
class Signal {
private:
    template<class K>
    using List = boost::intrusive::list<K, boost::intrusive::constant_time_size<false>>;
    using Hook = boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>;

public:
    class Connection : public Hook
    {
    public:
        Connection() = default;

        Connection(Connection&& other)
            : _slot(std::move(other._slot))
        {
            other.swap_nodes(*this);
        }

        Connection& operator=(Connection&& other) {
            _slot = std::move(other._slot);
            other.swap_nodes(*this);
            return *this;
        }

    private:
        friend class Signal;
        std::function<T> _slot;
    };

public:
    Signal()                    = default;

    Signal(const Signal&)            = delete;
    Signal& operator=(const Signal&) = delete;

    Signal(Signal&& other)
        : _connections(std::move(other._connections))
    {}

    Signal& operator=(Signal&& other)
    {
        _connections = std::move(other._connections);
        return *this;
    }

    template<typename... Args>
    void operator()(Args&&... args)
    {
        for (auto& connection : _connections) {
            try {
                connection._slot(std::forward<Args>(args)...);
            } catch (std::exception& e) {
                assert(0);
            }
        }
    }

    Connection connect(std::function<T> slot)
    {
        Connection connection;
        connection._slot = std::move(slot);
        _connections.push_back(connection);
        return connection;
    }

    size_t size() const { return _connections.size(); }

private:
    List<Connection> _connections;
};

} // namespace
