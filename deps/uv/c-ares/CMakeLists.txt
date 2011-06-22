include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${node_platform}-${cares_arch})
add_definitions(-DHAVE_CONFIG_H=1)

include(CheckLibraryExists)
check_library_exists(socket socket "" HAVE_SOCKET_LIB)
check_library_exists(nsl gethostbyname "" HAVE_NSL_LIB)

file(GLOB lib_sources *.c)
add_library(cares ${lib_sources})

if(${HAVE_SOCKET_LIB})
  set(cares_libs ${cares_libs} socket)
endif()

if(${HAVE_NSL_LIB})
  set(cares_libs ${cares_libs} nsl)
endif()

if(cares_libs)
  target_link_libraries(cares ${cares_libs})
endif()
