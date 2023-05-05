cmake_minimum_required(VERSION 3.25)
project(hello_cpp)

set(CMAKE_CXX_STANDARD 17)
include_directories(/usr/include/fuse3)

add_executable(hello_cpp main.cpp cxxopts.hpp)
target_link_libraries(hello_cpp fuse3 pthread)
