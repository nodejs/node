add_header_library(
  units
  HDRS
    units.h
  DEPENDS
    libc.src.__support.common
    libc.hdr.types.time_t
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS}
   AND TARGET libc.src.__support.OSUtil.osutil)
  add_subdirectory(${LIBC_TARGET_OS})
else()
  message(STATUS "Skip libc.src.__support.time.* targets.")
  return()
endif()

add_object_library(
  clock_gettime
  ALIAS
  DEPENDS
    libc.src.__support.time.${LIBC_TARGET_OS}.clock_gettime
)

add_header_library(
  clock_conversion
  HDRS
    clock_conversion.h
  DEPENDS
    .clock_gettime
    libc.src.__support.time.units
)

if(TARGET libc.src.__support.time.${LIBC_TARGET_OS}.clock_settime)  
  add_object_library(
    clock_settime
    ALIAS
    DEPENDS
      libc.src.__support.time.${LIBC_TARGET_OS}.clock_settime
  )
endif()

add_header_library(
  abs_timeout
  HDRS
    abs_timeout.h
  DEPENDS
    libc.hdr.types.struct_timespec
    libc.src.__support.time.units
    libc.src.__support.CPP.expected
)

add_header_library(
  monotonicity
  HDRS
    monotonicity.h
  DEPENDS
    .clock_conversion
    .abs_timeout
    libc.hdr.time_macros
)
