# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-src"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-build"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/tmp"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/src/snmalloc-populate-stamp"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/src"
  "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/src/snmalloc-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/src/snmalloc-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sevag/verona-rt/cmake-build-debug/_deps/snmalloc-subbuild/snmalloc-populate-prefix/src/snmalloc-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
