include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests test_orderbook.cpp)
target_link_libraries(unit_tests GTest::gtest_main)
target_include_directories(unit_tests PRIVATE ../include)

# Link the objects from your main project (exclude main.cpp)
target_sources(unit_tests PRIVATE
    ../src/orderbook.cpp
    ../src/orders_pool.cpp
)