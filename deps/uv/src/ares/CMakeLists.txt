cmake_minimum_required(VERSION 2.6)

project(ares)

string(TOLOWER ${CMAKE_SYSTEM_NAME} ares_platform)

include_directories(config_${ares_platform})
add_definitions(-DHAVE_CONFIG_H=1)

include(CheckLibraryExists)
check_library_exists(socket socket "" HAVE_SOCKET_LIB)
check_library_exists(nsl gethostbyname "" HAVE_NSL_LIB)

file(GLOB ares_source *.c)
add_library(ares ${ares_source})

if(HAVE_SOCKET_LIB)
  set(ares_libs ${ares_libs} socket)
endif()

if(HAVE_NSL_LIB)
  set(ares_libs ${ares_libs} nsl)
endif()

if(ares_libs)
  target_link_libraries(ares ${ares_libs})
endif()
