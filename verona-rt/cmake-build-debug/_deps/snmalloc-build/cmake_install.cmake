# Install script for directory: /home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/aal")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/ds")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/override")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/backend")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/mem")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE DIRECTORY FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/pal")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc/test" TYPE FILE FILES
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/test/measuretime.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/test/opt.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/test/setup.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/test/usage.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/test/xoroshiro.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/snmalloc" TYPE FILE FILES
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/snmalloc.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/snmalloc_core.h"
    "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src/src/snmalloc/snmalloc_front.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/snmalloc/snmalloc-config.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/snmalloc/snmalloc-config.cmake"
         "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-build/CMakeFiles/Export/43135d38e178c7a1b69df443136c9627/snmalloc-config.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/snmalloc/snmalloc-config-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/snmalloc/snmalloc-config.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/snmalloc" TYPE FILE FILES "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-build/CMakeFiles/Export/43135d38e178c7a1b69df443136c9627/snmalloc-config.cmake")
endif()

