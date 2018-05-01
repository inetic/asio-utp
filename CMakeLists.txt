cmake_minimum_required (VERSION 3.5)
set(BOOST_VERSION 1.58)
include(ExternalProject)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

set(SANITIZER_FLAGS "-fno-omit-frame-pointer -fsanitize=address")

set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}           ${SANITIZER_FLAGS}")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} ${SANITIZER_FLAGS}")

#---------------------------------------------------------------------
project(asio_utp)

set(CMAKE_CXX_STANDARD 14)
find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS system coroutine)
find_package(Threads)

set(CXX_INCLUDE_DIR "./include")

#---------------------------------------------------------------------

set(libutp_sources "./src/libutp/utp_internal.cpp"
                   "./src/libutp/utp_utils.cpp"
                   "./src/libutp/utp_hash.cpp"
                   "./src/libutp/utp_callbacks.cpp"
                   "./src/libutp/utp_api.cpp"
                   "./src/libutp/utp_packedsockaddr.cpp")

file(GLOB asio_utp_sources "./src/*.cpp")

set_source_files_properties(${libutp_sources} PROPERTIES COMPILE_FLAGS
    "-Wno-sign-compare -DPOSIX -fno-exceptions -fno-rtti -O3")

add_library(asio_utp ${asio_utp_sources} ${libutp_sources})

target_include_directories(asio_utp PUBLIC
    "${CXX_INCLUDE_DIR}"
    "./src/libutp"
    "${Boost_INCLUDE_DIR}")

#---------------------------------------------------------------------
file(GLOB ucat_sources "./example/*.cpp")
add_executable(ucat ${ucat_sources})
add_dependencies(ucat asio_utp)
target_link_libraries(ucat asio_utp ${Boost_LIBRARIES})

target_include_directories(ucat PUBLIC
    "${CXX_INCLUDE_DIR}"
    "${Boost_INCLUDE_DIR}")

#---------------------------------------------------------------------
# Tests
add_executable(test-util "test/util.cpp")
target_include_directories(test-util PUBLIC "./src" "${Boost_INCLUDE_DIR}")
target_link_libraries(test-util ${Boost_LIBRARIES})

# Tests
add_executable(test-comm "test/comm.cpp")
target_include_directories(test-comm PUBLIC "./src" "${Boost_INCLUDE_DIR}")
target_link_libraries(test-comm asio_utp ${Boost_LIBRARIES})

