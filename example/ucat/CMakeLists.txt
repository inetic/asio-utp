
file(GLOB ucat_sources "./ucat.cpp")
add_executable(ucat ${ucat_sources})
target_link_libraries(ucat asio_utp asio_utp_static_asio)
