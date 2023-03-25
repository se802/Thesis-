cmake_minimum_required(VERSION 3.24)
project(FuseProject C)

set(CMAKE_C_STANDARD 11)


include_directories(/usr/include/fuse3)

add_executable(FuseProject main.c main2.c)

target_link_libraries(FuseProject fuse3 pthread)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
