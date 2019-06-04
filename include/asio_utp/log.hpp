#pragma once

#include <iostream>
#include <assert.h>

namespace asio_utp {

namespace detail {
inline
void log_impl(std::ostream& ss) {
    ss << "\n";
}

template<class Arg, class... Args>
inline
void log_impl(std::ostream& ss, Arg&& arg, Args&&... args) {
    ss << arg;
    return log_impl(ss, std::forward<Args>(args)...);
}

extern std::ostream* g_logstream;

} // detail namespace

inline
void set_log_stream(std::ostream* logstream) {
    detail::g_logstream = logstream;
}

template<class... Args>
inline
void log(Args&&... args) {
    if (!detail::g_logstream) { return; }
    detail::log_impl(*detail::g_logstream, std::forward<Args>(args)...);
}

} // namespace
