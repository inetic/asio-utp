# Asio wrapper over the uTorrent's (MIT licensed) uTP library

# Advantages over TCP

* Multiple uTP connections over one UDP port implies
    * free hole-punching on certain types of NATs
    * fewer open file descriptors
* Low latency
* Yields to TCP

# Caveats

* An __accepting__ socket may only start sending **after** it received some data
  from the __connecting__ socket (likely a consequence of
  [this](https://github.com/bittorrent/libutp/issues/74))
* One has to implement their own timeouts and keep-alive packets because
  otherwise if the FIN UDP packet it gets dropped by the network then the
  remaining socket won't get destroyed. Note that there is a mention of
  keep-alive packets in `libutp/utp_internals.c`, but those seem to be only
  used for preserving holes in NATs (not to indicate whether the other end
  is still alive).

# TODO

* Multiple `utp::socket`s to share one `udp_loop`
* Add API to handle non-uTP packets
* Handle ICMP messages
* Thread safety
