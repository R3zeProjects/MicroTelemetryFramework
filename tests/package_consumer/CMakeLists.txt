cmake_minimum_required(VERSION 3.25)
project(mtf_package_consumer LANGUAGES CXX)

find_package(mtf 0.1 REQUIRED CONFIG)

add_executable(mtf_package_consumer main.cpp)
target_link_libraries(mtf_package_consumer PRIVATE vosp::telemetry)
