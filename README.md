# Asio wrapper over the uTorrent's (MIT licensed) uTP library

# Advantages over TCP

* Multiple uTP connections over one UDP port implies 
    * free hole-punching on certain types of NATs 
    * fewer open file descriptors
* Low latency
* Yields to TCP

# Caveats

* __Accepting__ socket may only start sending **after** it received some data
  from the __connecting__ socket (likely a consequence of
  [this](https://github.com/bittorrent/libutp/issues/74))

# TODO

* Thread safety
* Multiple `utp::socket`s to share one `udp_loop`
* Add API to handle non-uTP packets
* Handle ICMP messages
