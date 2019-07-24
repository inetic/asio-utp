
file(GLOB http_server_sources "./http_server_coro.cpp")
add_executable(http_server ${http_server_sources})
target_link_libraries(http_server asio_utp asio_utp_static_asio)

file(GLOB http_client_sources "./http_client_coro.cpp")
add_executable(http_client ${http_client_sources})
target_link_libraries(http_client asio_utp asio_utp_static_asio)
