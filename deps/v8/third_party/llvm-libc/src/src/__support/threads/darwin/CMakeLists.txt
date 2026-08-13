if(NOT TARGET libc.src.__support.OSUtil.osutil)
  return()
endif()

add_header_library(
  futex_utils
  HDRS
    futex_utils.h
  DEPENDS
    libc.include.sys_syscall
    libc.src.__support.OSUtil.osutil
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.limits
    libc.src.__support.CPP.optional
    libc.src.__support.threads.mutex_common
)
