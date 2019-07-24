cmake_minimum_required (VERSION 3.5)
set(BOOST_VERSION 1.58)
include(ExternalProject)

option(ASIO_UTP_WITH_TESTS    "Build tests" ON)
option(ASIO_UTP_WITH_EXAMPLES "Build examples" ON)
option(ASIO_UTP_DEBUG_LOGGING "Enable debug logging from asio_utp" OFF)
option(UTP_DEBUG_LOGGING      "Enable debug logging from libutp" OFF)

#---------------------------------------------------------------------

project(asio_utp)

set(CMAKE_CXX_STANDARD 14)

find_package(Threads)
find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS
    coroutine
    system
)

#---------------------------------------------------------------------

file(GLOB libutp_sources
    "src/libutp/utp_internal.cpp"
    "src/libutp/utp_utils.cpp"
    "src/libutp/utp_hash.cpp"
    "src/libutp/utp_callbacks.cpp"
    "src/libutp/utp_api.cpp"
    "src/libutp/utp_packedsockaddr.cpp"
)

file(GLOB asio_utp_sources "./src/*.cpp")

add_library(asio_utp STATIC ${asio_utp_sources} ${libutp_sources})
set_source_files_properties(${libutp_sources} PROPERTIES COMPILE_FLAGS
    "-Wno-sign-compare -DPOSIX -fno-exceptions -fno-rtti -O3"
)
target_include_directories(asio_utp
    PUBLIC "include"
    PRIVATE "src/libutp"
)
target_compile_definitions(asio_utp
    PUBLIC
        # This is a static library, so don't include asio. Users need to link
        # libboost_asio either statically or as a shared library.
        -DBOOST_ASIO_SEPARATE_COMPILATION
        # For some reason we need to define both of these
        -DBOOST_COROUTINES_NO_DEPRECATION_WARNING
        -DBOOST_COROUTINE_NO_DEPRECATION_WARNING
)
target_link_libraries(asio_utp
    PUBLIC Boost::boost
    INTERFACE ${Boost_LIBRARIES}
)

#---------------------------------------------------------------------

if (ASIO_UTP_DEBUG_LOGGING)
    target_compile_definitions(asio_utp
        PRIVATE -DASIO_UTP_DEBUG_LOGGING
    )
endif()

if (UTP_DEBUG_LOGGING)
    target_compile_definitions(asio_utp
        PRIVATE -DUTP_DEBUG_LOGGING=1
    )
endif()

#---------------------------------------------------------------------
# The static library asio_utp requires a separately compiled asio,
# so supply one for the tests and examples.
add_library(asio_utp_static_asio STATIC EXCLUDE_FROM_ALL "lib/asio.cpp")
target_link_libraries(asio_utp_static_asio
    PUBLIC
        Boost::boost
    INTERFACE
        ${Boost_LIBRARIES}
        Threads::Threads
)
target_compile_definitions(asio_utp_static_asio
    PUBLIC -DBOOST_ASIO_SEPARATE_COMPILATION
)

#---------------------------------------------------------------------
if(ASIO_UTP_WITH_EXAMPLES)
    # Boost.Beast example
    add_subdirectory(example/http)
    add_subdirectory(example/ucat)
endif() # ASIO_UTP_WITH_EXAMPLES

#---------------------------------------------------------------------
if(ASIO_UTP_WITH_TESTS)
    add_executable(test-util "test/util.cpp")
    target_include_directories(test-util PUBLIC "./src")
    target_link_libraries(test-util asio_utp_static_asio)

    add_executable(test-comm "test/comm.cpp")
    target_include_directories(test-comm PUBLIC "./src")
    target_link_libraries(test-comm asio_utp asio_utp_static_asio)

    add_executable(test-bench "test/bench.cpp")
    target_include_directories(test-bench PUBLIC "./src")
    target_link_libraries(test-bench asio_utp asio_utp_static_asio)
endif() # ASIO_UTP_WITH_TESTS
