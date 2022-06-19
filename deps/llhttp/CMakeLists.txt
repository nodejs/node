cmake_minimum_required(VERSION 3.5.1)
cmake_policy(SET CMP0069 NEW)

project(llhttp C)

set(CMAKE_C_STANDARD 99)

#
# Options
#
# Generic option
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" OFF)

# Source code
set(LLHTTP_SOURCES
  src/llhttp.c
  src/http.c
  src/api.c
)

set(LLHTTP_HEADERS
  include/llhttp.h
)

add_library(llhttp)
add_library(llhttp::llhttp ALIAS llhttp)

target_sources(llhttp PRIVATE ${LLHTTP_SOURCES} ${LLHTTP_HEADERS})

# On windows with Visual Studio, add a debug postfix so that release
# and debug libraries can coexist.
if(MSVC)
  set(CMAKE_DEBUG_POSTFIX "d")
endif()

target_include_directories(llhttp PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(llhttp PROPERTIES PUBLIC_HEADER ${LLHTTP_HEADERS})

install(TARGETS llhttp
        EXPORT llhttp
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/
)

# This is required to work with FetchContent
install(EXPORT llhttp
        FILE llhttp-config.cmake
        NAMESPACE llhttp::
        DESTINATION lib/cmake/llhttp)
