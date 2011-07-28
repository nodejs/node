cmake_minimum_required(VERSION 2.6)

project(ev)

add_definitions(-DEV_MULTIPLICITY=0)

string(TOLOWER ${CMAKE_SYSTEM_NAME} ev_platform)
set(config_header ${ev_SOURCE_DIR}/config_${ev_platform}.h)

if(EXISTS ${config_header})
  add_definitions(-DEV_CONFIG_H="${config_header}")
else()
  message(FATAL_ERROR "Unsupported platform: ${ev_platform}")
endif()

add_library (ev ev.c)
