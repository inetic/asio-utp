cmake_minimum_required (VERSION 3.5)
set(BOOST_VERSION 1.58)
include(ExternalProject)

option(ASIO_UTP_WITH_TESTS    "Build tests" ON)
option(ASIO_UTP_WITH_EXAMPLES "Build examples" ON)
option(ASIO_UTP_DEBUG_LOGGING "Enable debug logging from asio_utp" OFF)
option(UTP_DEBUG_LOGGING      "Enable debug logging from libutp" OFF)
option(ASIO_UTP_WITH_ASAN     "Build with Address sanitizer" OFF)

add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

# This is a static library, so don't include asio. Users need to link
# libboost_asio either statically or as a shared library.
add_definitions(-DBOOST_ASIO_SEPARATE_COMPILATION)

if (ASIO_UTP_WITH_ASAN)
    set(SANITIZER_FLAGS "-fno-omit-frame-pointer -fsanitize=address")
endif()

set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}           ${SANITIZER_FLAGS}")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} ${SANITIZER_FLAGS}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Wall ${CMAKE_CXX_FLAGS_DEBUG}")
endif()

if (ASIO_UTP_DEBUG_LOGGING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_UTP_DEBUG_LOGGING")
endif()

if (UTP_DEBUG_LOGGING)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -DUTP_DEBUG_LOGGING=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUTP_DEBUG_LOGGING=1")
endif()

#---------------------------------------------------------------------
project(asio_utp)

set(CMAKE_CXX_STANDARD 14)
find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS system coroutine)
find_package(Threads)

set(CXX_INCLUDE_DIR "./include")

#---------------------------------------------------------------------

set(libutp_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_internal.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_utils.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_hash.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_callbacks.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_api.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/src/libutp/utp_packedsockaddr.cpp")

file(GLOB asio_utp_sources "./src/*.cpp")

set_source_files_properties(${libutp_sources} PROPERTIES COMPILE_FLAGS
    "-Wno-sign-compare -DPOSIX -fno-exceptions -fno-rtti -O3")

add_library(asio_utp ${asio_utp_sources} ${libutp_sources})

target_include_directories(asio_utp PUBLIC
    "${CXX_INCLUDE_DIR}"
    "./src/libutp"
    "${Boost_INCLUDE_DIR}")

#---------------------------------------------------------------------
# The static library asio_utp requires a separately compiled asio,
# so supply one for the tests and examples.
add_library(asio_utp_static_asio STATIC EXCLUDE_FROM_ALL "lib/asio.cpp")
target_link_libraries(asio_utp_static_asio ${Boost_LIBRARIES})

#---------------------------------------------------------------------
if(ASIO_UTP_WITH_EXAMPLES)
    # Boost.Beast example
    add_subdirectory(example/http)
    add_subdirectory(example/ucat)
endif() # ASIO_UTP_WITH_EXAMPLES

#---------------------------------------------------------------------
if(ASIO_UTP_WITH_TESTS)
    # Tests
    add_executable(test-util "test/util.cpp")
    target_include_directories(test-util PUBLIC "./src" "${Boost_INCLUDE_DIR}")
    target_link_libraries(test-util ${Boost_LIBRARIES} Threads::Threads asio_utp_static_asio)

    # Tests
    add_executable(test-comm "test/comm.cpp")
    target_include_directories(test-comm PUBLIC "./src" "${Boost_INCLUDE_DIR}")
    target_link_libraries(test-comm asio_utp ${Boost_LIBRARIES} Threads::Threads asio_utp_static_asio)

    # Tests
    add_executable(test-bench "test/bench.cpp")
    target_include_directories(test-bench PUBLIC "./src" "${Boost_INCLUDE_DIR}")
    target_link_libraries(test-bench asio_utp ${Boost_LIBRARIES} Threads::Threads asio_utp_static_asio)
endif() # ASIO_UTP_WITH_TESTS
