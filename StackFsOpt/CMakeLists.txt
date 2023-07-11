cmake_minimum_required(VERSION 3.25)
project(StackFsOpt C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall ")
set(CMAKE_BUILD_TYPE Debug)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED fuse3)

add_executable(StackFsOpt main.c)

target_include_directories(StackFsOpt PRIVATE ${FUSE_INCLUDE_DIRS})
target_link_libraries(StackFsOpt ${FUSE_LIBRARIES})
target_compile_options(StackFsOpt PRIVATE ${FUSE_CFLAGS})
target_compile_definitions(StackFsOpt PRIVATE FUSE_USE_VERSION=34)