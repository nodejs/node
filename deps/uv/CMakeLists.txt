cmake_minimum_required(VERSION 2.6)

project(uv)

string(TOLOWER ${CMAKE_SYSTEM_NAME} uv_platform)

set(uv_platform_source ${uv_SOURCE_DIR}/src/uv-${uv_platform}.c)

set(uv_source
  src/uv-common.c
  src/uv-eio.c
  src/uv-unix.c)

if(EXISTS ${uv_platform_source})
  set(uv_source ${uv_source} ${uv_platform_source})
else()
  message(FATAL_ERROR "Unsupported platform: ${uv_platform}")
endif()

include_directories(
  include
  src/ares/config_${uv_platform})

add_subdirectory(src/ev)
add_subdirectory(src/eio)
add_subdirectory(src/ares)

add_library(uv ${uv_source})
target_link_libraries(uv
  ev
  eio
  ares)
