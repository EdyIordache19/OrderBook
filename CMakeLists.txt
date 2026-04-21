cmake_minimum_required(VERSION 3.14)
project(OrderBook LANGUAGES CXX)

# 1. Standard Settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Optimization Flags (The "Release" build)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Flags: -O3 for speed, -march=native for CPU instructions, -flto for link-time optimization
if(MSVC)
    add_compile_options(/O2 /Oi /Ot)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra -pthread)
endif()

# 3. Handle cxxopts (Header-only library)
# We assume it is in include/, or we can fetch it automatically (Advanced).
# For now, just ensuring include/ is in the path works.

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

# Add tests subdirectory
add_subdirectory(tests)