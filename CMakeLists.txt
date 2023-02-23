cmake_minimum_required(VERSION 3.16)
project(radiation_test)

set(CMAKE_CXX_STANDARD 14)

add_executable(radiation_test main.cpp)
target_compile_options(radiation_test PUBLIC -O3)
target_link_options(radiation_test PUBLIC -pthread -s)
