cmake_minimum_required(VERSION 3.20)

project(ledstripesp32_tests LANGUAGES CXX)

find_package(Catch2 CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

include_directories(../lib/color/src)

add_executable(tests main.cpp colours.cpp)
# target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PUBLIC Catch2::Catch2 Eigen3::Eigen)


# Include catch2 headers


include(CTest)
include(Catch)
catch_discover_tests(tests)

