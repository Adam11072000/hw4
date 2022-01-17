cmake_minimum_required(VERSION 3.21)
project(hw4)

set(CMAKE_CXX_STANDARD 11)

set(FLAGS "-fpermissive")

add_definitions(${FLAGS})

add_executable(hw4 malloc_3.cpp os_hw4_part3_tests/test.cpp)
