cmake_minimum_required(VERSION 3.20)
project(reverse-proxy CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

include_directories(${CMAKE_SOURCE_DIR})

# --- Main proxy executable ---
add_executable(proxy
    src/main.cpp
    src/http/parser.cpp
    src/proxy/forwarder.cpp
    src/server/server.cpp
    src/logging/logger.cpp
)

# --- Unit tests (HTTP parser only, no network) ---
enable_testing()
add_executable(test_parser
    tests/test_parser.cpp
    src/http/parser.cpp
    src/logging/logger.cpp
)
target_include_directories(test_parser PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_parser COMMAND test_parser)

# --- Integration tests (parser + forwarder, needs backend running) ---
add_executable(test_integration
    tests/test_integration.cpp
    src/http/parser.cpp
    src/proxy/forwarder.cpp
    src/logging/logger.cpp
)
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME test_integration COMMAND test_integration)
