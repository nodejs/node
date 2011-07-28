cmake_minimum_required(VERSION 2.6)

include(FindThreads)

project(eio)

add_definitions(-D_GNU_SOURCE)

if(!${CMAKE_USE_PTHREADS_INIT})
    message(FATAL_ERROR "Unable to find pthreads")
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME} eio_platform)
set(config_header ${ev_SOURCE_DIR}/config_${eio_platform}.h)

if(EXISTS ${config_header})
  add_definitions(-DEIO_CONFIG_H="${config_header}")
else()
  message(FATAL_ERROR "Unsupported platform: ${eio_platform}")
endif()

add_library(eio eio.c)
target_link_libraries(eio ${CMAKE_THREAD_LIBS_INIT})
