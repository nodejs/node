include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${node_platform}-${cares_arch})
add_definitions(-DHAVE_CONFIG_H=1)

file(GLOB lib_sources *.c)
add_library(cares ${lib_sources})