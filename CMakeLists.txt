cmake_minimum_required(VERSION 2.6)
project(node)

if(USE_GCOV)
  set(CMAKE_BUILD_TYPE "Debug")

  # Set global c and c++ flags
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")

  # Link flags used for creating executables
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov -fprofile-arcs")

  # Link flags used for creating shared libraries
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lgcov -profile-arcs")
endif()

#
# options
#

find_package(PythonInterp 2 REQUIRED)

option(SHARED_V8 "use system shared V8 library")
option(SHARED_LIBEV "use system shared libev library")
option(SHARED_CARES "use system shared c-ares library")
option(V8_SNAPSHOT "turn on snapshot when building stock v8")
option(V8_OPROFILE "Add oprofile support")
option(V8_GDBJIT "add gdbjit support")
option(DTRACE "build with DTrace (experimental)")

# cmake policies to get rid of some warnings
cmake_policy(SET CMP0009 NEW) # GLOB_RECURSE should no follow symlinks

# generic cmake configuration
include("cmake/configure.cmake")

# find and configure libs
include("cmake/libs.cmake")

# setup node build targets
include("cmake/node_build.cmake")

# setup v8 build targets
include("cmake/v8_build.cmake")

# docs
## might want to move this to doc/CMakeLists.txt
include("cmake/docs.cmake")

# tests
enable_testing()
include(CTest)
add_subdirectory("test/")

# package
include("cmake/package.cmake")


#
# Final build configuration output
#

message("** Build Summary **")
message("  Version:            ${node_version_string}")
message("  Prefix:             ${PREFIX}")
message("  Build Type:         ${CMAKE_BUILD_TYPE}")
message("  Architecture:       ${CMAKE_SYSTEM_PROCESSOR}")
message("  Platform:           ${node_platform}")

if(SHARED_V8)
message("  V8:                 ${V8_LIBRARY_PATH}")
#else()
#message(" V8 jobs:            ${parallel_jobs}")
endif()

if(SHARED_libev)
  message("  libev:              ${LIBEV_LIBRARY}")
endif()

if(SHARED_CARES)
  message("  libc-ares:          ${LIBCARES_LIBRARY}")
endif()

message("  RT library:         ${RT}")
message("  DL library:         ${DL}")

if(${OPENSSL_FOUND} MATCHES TRUE)
  message("  OpenSSL:            ${OPENSSL_LIBRARIES}")
endif()

if(USE_GCOV)
  message("  gcov:               enabled")
endif()

message("  CCFLAGS:            ${CCFLAGS}")
message("  CPPFLAGS:           ${CPPFLAGS}")
