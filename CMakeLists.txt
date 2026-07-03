cmake_minimum_required(VERSION 3.14)
project(OrderBook LANGUAGES CXX)

# 1. Standard Settings - UPGRADED TO C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Interprocedural Optimization (LTO) globally
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# 2. Optimization Flags (The "Release" build)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/O2 /Oi /Ot)
else()
    # Added -fno-exceptions and -fno-rtti for HFT compliance
    add_compile_options(-O3 -march=native -Wall -Wextra -pthread)
endif()

# 4. Define the Executable
add_executable(orderbook
    src/main.cpp
    src/orderbook.cpp
    src/orders_generator.cpp
    src/orders_pool.cpp
    src/engine.cpp
    src/gateway.cpp
    src/decoder.cpp
    src/publisher.cpp
)

# 5. Include Directories
target_include_directories(orderbook PRIVATE include)

# 6. Client Executable (The UDP Blaster)
add_executable(client apps/client.cpp)
target_include_directories(client PRIVATE include)

add_subdirectory(tests)