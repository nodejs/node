set(LIBC_INCLUDE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBC_INCLUDE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

include(LLVMLibCHeaderRules)

function(add_proxy_header_library target_name)
  cmake_parse_arguments(
    "ADD_PROXY_HEADER"
    "" # Optional arguments
    "" # Single value arguments
    "DEPENDS;FULL_BUILD_DEPENDS" # Multi-value arguments
    ${ARGN}
  )

  set(deps "")
  if(ADD_PROXY_HEADER_DEPENDS)
    list(APPEND deps ${ADD_PROXY_HEADER_DEPENDS})
  endif()

  if(LLVM_LIBC_FULL_BUILD AND ADD_PROXY_HEADER_FULL_BUILD_DEPENDS)
    list(APPEND deps ${ADD_PROXY_HEADER_FULL_BUILD_DEPENDS})
  endif()

  add_header_library(
    ${target_name}
    ${ADD_PROXY_HEADER_UNPARSED_ARGUMENTS}
    DEPENDS ${deps}
  )
endfunction()

add_proxy_header_library(
  math_macros
  HDRS
    math_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.math_macros
)

add_proxy_header_library(
  math_function_macros
  HDRS
    math_function_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.math_function_macros
)

add_proxy_header_library(
  errno_macros
  HDRS
    errno_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.error_number_macros
    libc.include.llvm-libc-macros.generic_error_number_macros
)

add_header_library(fcntl_overlay HDRS fcntl_overlay.h)
add_proxy_header_library(
  fcntl_macros
  HDRS
    fcntl_macros.h
  DEPENDS
    .fcntl_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.fcntl_macros
)

add_proxy_header_library(
  fenv_macros
  HDRS
    fenv_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.fenv_macros
)

add_proxy_header_library(
  inet_address_macros
  HDRS
    inet-address-macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.inet_address_macros
)

add_proxy_header_library(
  net_if_macros
  HDRS
    net_if_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.net_if_macros
)

add_proxy_header_library(
  netinet_in_macros
  HDRS
    netinet_in_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.netinet_in_macros
)

add_proxy_header_library(
  pthread_macros
  HDRS
    pthread_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.pthread_macros
)

add_proxy_header_library(
  sched_macros
  HDRS
    sched_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sched_macros
)

add_proxy_header_library(
  signal_macros
  HDRS
    signal_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.signal_macros
)

add_header_library(stdlib_overlay HDRS stdlib_overlay.h)

add_proxy_header_library(
  stdlib_macros
  HDRS
    stdlib_macros.h
  DEPENDS
    .stdlib_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.stdlib_macros
)

add_header_library(dirent_overlay HDRS dirent_overlay.h)
add_header_library(stdio_overlay HDRS stdio_overlay.h)

add_proxy_header_library(
  stdio_macros
  HDRS
    stdio_macros.h
  DEPENDS
    .stdio_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.stdio_macros
    libc.include.llvm-libc-macros.file_seek_macros
)

add_proxy_header_library(
  sys_epoll_macros
  HDRS
    sys_epoll_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_epoll_macros
)

add_proxy_header_library(
  sys_ioctl_macros
  HDRS
    sys_ioctl_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_ioctl_macros
)

add_proxy_header_library(
  sys_ipc_macros
  HDRS
    sys_ipc_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_ipc_macros
)

add_proxy_header_library(
  sys_select_macros
  HDRS
    sys_select_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_select_macros
)

add_proxy_header_library(
  sys_sem_macros
  HDRS
    sys_sem_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_sem_macros
)

add_proxy_header_library(
  sys_socket_macros
  HDRS
    sys_socket_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_socket_macros
)

add_proxy_header_library(
  sys_stat_macros
  HDRS
    sys_stat_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_stat_macros
)

add_proxy_header_library(
  sys_mman_macros
  HDRS
    sys_mman_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_mman_macros
)

add_proxy_header_library(
  sys_ptrace_macros
  HDRS
    sys_ptrace_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_ptrace_macros
)

add_proxy_header_library(
  sys_time_macros
  HDRS
    sys_time_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_time_macros
)

add_proxy_header_library(
  sys_wait_macros
  HDRS
    sys_wait_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_wait_macros
)

add_proxy_header_library(
  termios_macros
  HDRS
    termios_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.termios_macros
)

add_header_library(unistd_overlay HDRS unistd_overlay.h)
add_proxy_header_library(
  unistd_macros
  HDRS
    unistd_macros.h
  DEPENDS
    .unistd_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.unistd_macros
)

if(WIN32)
  set(windows_addtional_time_macros libc.include.llvm-libc-macros.windows.time_macros_ext)
else()
  set(windows_addtional_time_macros "")
endif()

add_proxy_header_library(
  time_macros
  HDRS
    time_macros.h
  DEPENDS
    ${windows_addtional_time_macros}
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.time_macros
)

add_proxy_header_library(
  float_macros
  HDRS
    float_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.float_macros
)

add_proxy_header_library(
  limits_macros
  HDRS
    limits_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.limits_macros
)

add_proxy_header_library(
  link_macros
  HDRS
    link_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.link_macros
)

add_proxy_header_library(
  locale_macros
  HDRS
    locale_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.locale_macros
)

add_proxy_header_library(
  sys_auxv_macros
  HDRS
    sys_auxv_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_auxv_macros
)

add_proxy_header_library(
  sys_resource_macros
  HDRS
    sys_resource_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_resource_macros
)

add_header_library(wchar_overlay HDRS wchar_overlay.h)

add_header_library(uchar_overlay HDRS uchar_overlay.h)

add_proxy_header_library(
  wchar_macros
  HDRS
    wchar_macros.h
  DEPENDS
    .wchar_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.wchar_macros
)

# offsetof is a macro inside compiler resource header stddef.h
add_proxy_header_library(
  offsetof_macros
  HDRS
    offsetof_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.offsetof_macro
)

# stdint.h header.
add_proxy_header_library(
  stdint_proxy
  HDRS
    stdint_proxy.h
  FULL_BUILD_DEPENDS
    libc.include.stdint
)

add_proxy_header_library(
  elf_macros
  HDRS
    elf_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.elf_macros
)

add_proxy_header_library(
  elf_proxy
  HDRS
    elf_proxy.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-proxy.elf_proxy
)

add_proxy_header_library(
  regex_macros
  HDRS
    regex_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.regex_macros
)

add_subdirectory(types)
add_subdirectory(func)
