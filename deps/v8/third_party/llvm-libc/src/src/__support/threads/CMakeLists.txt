add_header_library(
  mutex_common
  HDRS
    mutex_common.h
)

add_header_library(
  sleep
  HDRS
    sleep.h
)

add_header_library(
  spin_lock
  HDRS
    spin_lock.h
  DEPENDS
    .sleep
    libc.src.__support.CPP.atomic
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  add_subdirectory(${LIBC_TARGET_OS})
endif()

if(TARGET libc.src.__support.threads.${LIBC_TARGET_OS}.futex_utils)
  add_header_library(
    futex_utils
    HDRS
      futex_utils.h
    DEPENDS
      .${LIBC_TARGET_OS}.futex_utils
  )

  libc_set_definition(rwlock_default_spin_count "LIBC_COPT_RWLOCK_DEFAULT_SPIN_COUNT=${LIBC_CONF_RWLOCK_DEFAULT_SPIN_COUNT}")

  add_header_library(
    raw_mutex
    HDRS
      raw_mutex.h
    DEPENDS
      .futex_utils
      libc.src.__support.threads.sleep
      libc.src.__support.time.abs_timeout
      libc.src.__support.time.monotonicity
      libc.src.__support.CPP.optional
      libc.hdr.types.pid_t
  )

  add_header_library(
    raw_rwlock
    HDRS
      raw_rwlock.h
    COMPILE_OPTIONS
      ${rwlock_default_spin_count}
    DEPENDS
      .raw_mutex
      libc.src.__support.common
      libc.src.__support.CPP.limits
  )

  add_header_library(
    unix_rwlock
    HDRS
      unix_rwlock.h
    DEPENDS
      .raw_rwlock
      libc.hdr.types.pid_t
      libc.src.__support.threads.identifier
  )

  add_header_library(
    unix_mutex
    HDRS
      unix_mutex.h
    DEPENDS
      .raw_mutex
      libc.src.__support.CPP.atomic
  )

  add_header_library(
    mutex
    HDRS
      mutex.h
    DEPENDS
      .unix_mutex
  )

  add_object_library(
    fork_callbacks
    SRCS
      fork_callbacks.cpp
    HDRS
      fork_callbacks.h
    DEPENDS
      .mutex
      libc.src.__support.CPP.mutex
  )
elseif(NOT (LIBC_CONF_THREAD_MODE STREQUAL LIBC_THREAD_MODE_PLATFORM))
  add_header_library(
    mutex
    HDRS
      mutex.h
    DEPENDS
      .mutex_common
  )
endif()

add_header_library(
  thread_common
  HDRS
    thread.h
  DEPENDS
    libc.hdr.stdint_proxy
    libc.src.__support.common
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.optional
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.stringstream
)

if(TARGET libc.src.__support.threads.${LIBC_TARGET_OS}.thread)
  add_object_library(
    thread
    SRCS
      thread.cpp
    DEPENDS
      .mutex
      .${LIBC_TARGET_OS}.thread
      libc.hdr.stdint_proxy
      libc.src.__support.common
      libc.src.__support.fixedvector
      libc.src.__support.CPP.array
      libc.src.__support.CPP.mutex
      libc.src.__support.CPP.optional
  )
endif()

if(TARGET libc.src.__support.threads.${LIBC_TARGET_OS}.callonce)
  add_object_library(
    callonce
    ALIAS
    DEPENDS
      .${LIBC_TARGET_OS}.callonce
  )
endif()

if(TARGET libc.src.__support.threads.futex_utils)
  add_header_library(
    CndVar
    HDRS
      CndVar.h
    DEPENDS
      .futex_utils
      .mutex
      .mutex_common
      .raw_mutex
      libc.hdr.stdint_proxy
      libc.src.__support.CPP.expected
      libc.src.__support.CPP.limits
      libc.src.__support.CPP.mutex
      libc.src.__support.CPP.new
      libc.src.__support.threads.sleep
      libc.src.__support.time.monotonicity
      libc.src.__support.time.abs_timeout
      libc.src.__support.threads.identifier
  )
endif()

if (LLVM_LIBC_FULL_BUILD)
  set(identifier_dependency_on_thread libc.src.__support.threads.thread)
endif()

add_header_library(
  identifier
  HDRS
    identifier.h
  DEPENDS
    libc.src.__support.OSUtil.osutil
    libc.src.__support.common
    libc.include.sys_syscall
    libc.hdr.types.pid_t
    ${identifier_dependency_on_thread}
)
