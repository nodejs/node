add_header_library(
  futex_word_type
  HDRS
    futex_word.h
  DEPENDS
    libc.hdr.stdint_proxy
)

if(NOT TARGET libc.src.__support.OSUtil.osutil)
  return()
endif()

add_header_library(
  futex_utils
  HDRS
    futex_utils.h
  DEPENDS
    .futex_word_type
    libc.include.sys_syscall
    libc.src.__support.OSUtil.osutil
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.limits
    libc.src.__support.CPP.optional
    libc.src.__support.time.abs_timeout
)

add_object_library(
  thread
  SRCS
    thread.cpp
  DEPENDS
    .futex_utils
    libc.config.app_h
    libc.include.sys_syscall
    libc.hdr.fcntl_macros
    libc.hdr.errno_macros
    libc.src.errno.errno
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.stringstream
    libc.src.__support.CPP.string_view
    libc.src.__support.common
    libc.src.__support.error_or
    libc.src.__support.threads.thread_common
  COMPILE_OPTIONS
    ${libc_opt_high_flag}
    -fno-omit-frame-pointer # This allows us to sniff out the thread args from
                            # the new thread's stack reliably.
    -Wno-frame-address      # Yes, calling __builtin_return_address with a
                            # value other than 0 is dangerous. We know.
)

add_header_library(
  callonce
  HDRS
    ../callonce.h
    callonce.h
  DEPENDS
    .futex_utils
    libc.src.__support.macros.optimization
)

add_object_library(
  barrier
  HDRS
    barrier.h
  SRCS
    barrier.cpp
  DEPENDS
    libc.src.__support.threads.CndVar
    libc.src.__support.threads.mutex
)
