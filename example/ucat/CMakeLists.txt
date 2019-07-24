
file(GLOB ucat_sources "./*.cpp")
add_executable(ucat ${ucat_sources})
add_dependencies(ucat asio_utp)
target_link_libraries(ucat asio_utp ${Boost_LIBRARIES} Threads::Threads asio_utp_static_asio)

target_include_directories(ucat PUBLIC
    "${CXX_INCLUDE_DIR}"
    "${Boost_INCLUDE_DIR}")
