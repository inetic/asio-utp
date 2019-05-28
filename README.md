[![CircleCI](https://circleci.com/gh/inetic/asio-utp/tree/master.svg?style=shield)](https://circleci.com/gh/inetic/asio-utp/tree/master)

# Asio wrapper over the uTorrent's (MIT licensed) uTP library

## Features

Similar API to the TCP sockets in Asio.  In particular, the `utp::socket`
implements the [`AsyncReadStream`] and [`AsyncWriteStream`] requirements.
Making it readily usable with Asio's free functions or classes utilizing those
requirements. Such as [`async_read`], [`async_write`], [`ssl::stream`], ...

Also similar to Asio's TCP sockets, `utp::socket`'s `async` API supports
callbacks, futures and coroutines as completion tokens.

The `asio_utp::udp_multiplexer` can be used to perform non uTP sending and
receiving of UDP datagrams.

## Advantages of uTP over TCP

* Multiple uTP connections over one UDP port implies
    * better options to do NAT hole-punching
    * free hole-punching on certain types of NATs
    * fewer open file descriptors
* Low latency
* Yields to TCP

## Clone

`asio-utp` git repository contains `libutp` (the uTorrents uTP library written in C)
as a submodule. Thus one has to clone it recursively:

```
git clone --recursive git@github.com:inetic/asio-utp.git
```

## Build

```
cd asio-utp
mkdir build
cd build
cmake .. -DBOOST_ROOT=<PATH_TO_BOOST_DIRECTORY>
make -j$(nproc)
```

For more detailed instructions, have a look at the `.circleci/config.yml` file.

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
* The call to `socket::async_connect` simply executes the underlying
  `libutp/utp_connect` function. The latter sends one SYN packet but does not
  implement any timeout nor resending of that packet. This needs to be done
  explicitly by closing the socket after some timeout and then re-starting
  the call to `socket::async_connect`.

## Architecture

```
    asio_utp::socket --- asio_utp::socket_impl ---+
                                                   \
    asio_utp::socket --- asio_utp::socket_impl -----+--- asio_utp::context
                                                   /             \
    asio_utp::socket --- asio_utp::socket_impl ---+               \
                                                                   \
                                          +----------- asio_utp::udp_multiplexer_impl
    asio_utp::udp_multiplexer ---+       /                              \
                                  \     /                                \
    asio_utp::udp_multiplexer -----+---+                          asio::udp::socket
                                  /
    asio_utp::udp_multiplexer ---+
```

## TODO

* Handle ICMP messages
* Thread safety


[`AsyncReadStream`]:  https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/AsyncReadStream.html
[`AsyncWriteStream`]: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/AsyncWriteStream.html
[`async_read`]:       https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/async_read.html
[`async_write`]:      https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/async_write.html
[`ssl::stream`]:      https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/reference/ssl__stream.html
[API]:                https://github.com/inetic/asio-utp/blob/master/include/asio_utp/socket.hpp#L15
