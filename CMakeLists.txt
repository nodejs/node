cmake_minimum_required(VERSION 2.6)
project(node)

#
# options
#

option(SHARED_V8 "use system shared V8 library")
option(SHARED_LIBEV "use system shared libev library")
option(SHARED_CARES "use system shared c-ares library")
option(V8_SNAPSHOT "turn on snapshot when building stock v8")


# cmake policies to get rid of some warnings
cmake_policy(SET CMP0009 NEW)

# generic cmake configuration
include("cmake/configure.cmake")

# find and configure libs
include("cmake/libs.cmake")

# setup node build targets
include("cmake/node_build.cmake")

# setup v8 build targets
include("cmake/v8_build.cmake")

# docs
include("cmake/docs.cmake")

# tests
include("cmake/tests.cmake")

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
  message("  OpenSSL:            Found!")
endif()

# message("  CCFLAGS:            ${CCFLAGS}")
# message("  CPPFLAGS:           ${CPPFLAGS}")
