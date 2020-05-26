#pragma once

#include <boost/intrusive/list.hpp>

namespace asio_utp { namespace intrusive {

using list_hook = boost::intrusive::list_base_hook
                  <boost::intrusive::link_mode
                      <boost::intrusive::auto_unlink>>;

template<class Item, list_hook Item::* HookPtr>
using list = boost::intrusive::list
        < Item
        , boost::intrusive::member_hook<Item, list_hook, HookPtr>
        , boost::intrusive::constant_time_size<false>
        >;

}} // namespace
