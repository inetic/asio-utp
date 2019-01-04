
file(GLOB http_server_sources "./http_server_coro.cpp")
add_executable(http_server ${http_server_sources})
add_dependencies(http_server asio_utp)
target_link_libraries(http_server asio_utp ${Boost_LIBRARIES} Threads::Threads)

target_include_directories(http_server PUBLIC
    "${CXX_INCLUDE_DIR}"
    "${Boost_INCLUDE_DIR}")

file(GLOB http_client_sources "./http_client_coro.cpp")
add_executable(http_client ${http_client_sources})
add_dependencies(http_client asio_utp)
target_link_libraries(http_client asio_utp ${Boost_LIBRARIES} Threads::Threads)

target_include_directories(http_client PUBLIC
    "${CXX_INCLUDE_DIR}"
    "${Boost_INCLUDE_DIR}")

