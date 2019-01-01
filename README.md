[![CircleCI](https://circleci.com/gh/inetic/asio-utp/tree/master.svg?style=shield)](https://circleci.com/gh/inetic/asio-utp/tree/master)

# Asio wrapper over the uTorrent's (MIT licensed) uTP library

## Features

Similar API to the TCP sockets in Asio.  In particular, the `utp::socket`
implements the [`AsyncReadStream`] and [`AsyncWriteStream`] requirements.
Making it readily usable with Asio's free functions or classes utilizing those
requirements. Such as [`async_read`], [`async_write`], [`ssl::stream`], ...

Also similar to Asio's TCP sockets, `utp::socket`'s `async` API supports
callbacks, futures and coroutines as completion tokens.

## Advantages of uTP over TCP

* Multiple uTP connections over one UDP port implies
    * better options to do NAT hole-punching
    * free hole-punching on certain types of NATs
    * fewer open file descriptors
* Low latency
* Yields to TCP


## Caveats

* An __accepting__ socket may only start sending **after** it received some data
  from the __connecting__ socket (likely a consequence of
  [this](https://github.com/bittorrent/libutp/issues/74))
* One has to implement their own timeouts and keep-alive packets because
  otherwise if the FIN UDP packet gets dropped by the network then the
  remaining socket won't get destroyed. Note that there is a mention of
  keep-alive packets in `libutp/utp_internals.c`, but those seem to be only
  used for preserving holes in NATs (not to indicate whether the other end
  is still alive).


## TODO

* Add API to handle non-uTP packets
* Handle ICMP messages
* Thread safety
* The [API] is pretty minimal and mostly identical to the one in `boost::ip::tcp::socket`, but some
  documentation wouls still be in place

[`AsyncReadStream`]:  https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/AsyncReadStream.html
[`AsyncWriteStream`]: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/AsyncWriteStream.html
[`async_read`]:       https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/async_read.html
[`async_write`]:      https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/async_write.html
[`ssl::stream`]:      https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/ssl__stream.html
[API]:                https://github.com/inetic/asio-utp/blob/master/include/utp/socket.hpp#L15
