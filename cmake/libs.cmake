#
# libraries
#

include(CheckLibraryExists)
include(FindPackageHandleStandardArgs)
set(HAVE_CONFIG_H True)
add_definitions(-DHAVE_CONFIG_H=1)

find_package(OpenSSL QUIET)
find_package(Threads)
find_library(RT rt)
find_library(DL dl)
check_library_exists(socket socket "" HAVE_SOCKET_LIB)
check_library_exists(nsl gethostbyname "" HAVE_NSL_LIB)
check_library_exists(util openpty "" HAVE_UTIL_LIB)

if(RT)
  set(extra_libs ${extra_libs} ${RT})
endif()

if(DL)
  set(extra_libs ${extra_libs} ${DL})
endif()

if(${node_platform} MATCHES freebsd)
  find_library(KVM NAMES kvm)
  set(extra_libs ${extra_libs} KVM)
endif()

if(${HAVE_SOCKET_LIB})
  set(extra_libs ${extra_libs} socket)
endif()

if(${HAVE_NSL_LIB})
  set(extra_libs ${extra_libs} nsl)
endif()

if(HAVE_UTIL_LIB)
  set(extra_libs ${extra_libs} util)
endif()

if(OPENSSL_FOUND)
  add_definitions(-DHAVE_OPENSSL=1)
  set(HAVE_OPENSSL True)
  set(node_extra_src ${node_extra_src} src/node_crypto.cc)
  set(extra_libs ${extra_libs} ${OPENSSL_LIBRARIES})
endif()

include("cmake/libc-ares.cmake")
include("cmake/libev.cmake")
include("cmake/libv8.cmake")

add_subdirectory(deps/libeio)
add_subdirectory(deps/http_parser)
