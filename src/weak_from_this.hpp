#pragma once

#include <memory>

namespace asio_utp {

template<class T> std::weak_ptr<T> weak_from_this(T* p) {
#if __cplusplus > 201402L // c++1z and c++17
    return p->weak_from_this();
#else
    return p->shared_from_this();
#endif
}

}
