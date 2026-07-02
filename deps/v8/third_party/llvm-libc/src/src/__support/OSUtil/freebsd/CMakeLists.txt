if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_ARCHITECTURE})
  return()
endif()

add_subdirectory(${LIBC_TARGET_ARCHITECTURE})
add_subdirectory(syscall_wrappers)

add_object_library(
  freebsd_util
  SRCS
    exit.cpp
  HDRS
    io.h
  DEPENDS
    .${LIBC_TARGET_ARCHITECTURE}.freebsd_${LIBC_TARGET_ARCHITECTURE}_util
    libc.src.__support.OSUtil.freebsd.syscall_wrappers.write
    libc.src.__support.macros.config
)
