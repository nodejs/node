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
    libc.include.math
)

add_proxy_header_library(
  math_function_macros
  HDRS
    math_function_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.math_function_macros
    libc.include.math
)

add_proxy_header_library(
  errno_macros
  HDRS
    errno_macros.h
  FULL_BUILD_DEPENDS
    libc.include.errno
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
    libc.include.fcntl
)

add_proxy_header_library(
  fenv_macros
  HDRS
    fenv_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.fenv_macros
    libc.include.fenv
)

add_proxy_header_library(
  pthread_macros
  HDRS
    pthread_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.pthread_macros
    libc.include.pthread
)

add_proxy_header_library(
  sched_macros
  HDRS
    sched_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sched
    libc.include.llvm-libc-macros.sched_macros
)

add_proxy_header_library(
  signal_macros
  HDRS
    signal_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.signal_macros
    libc.include.signal
)

add_header_library(stdlib_overlay HDRS stdlib_overlay.h)

add_proxy_header_library(
  stdlib_macros
  HDRS
    stdlib_macros.h
  DEPENDS
    .stdlib_overlay
  FULL_BUILD_DEPENDS
    libc.include.stdlib
    libc.include.llvm-libc-macros.stdlib_macros
)

add_header_library(stdio_overlay HDRS stdio_overlay.h)

add_proxy_header_library(
  stdio_macros
  HDRS
    stdio_macros.h
  DEPENDS
    .stdio_overlay
  FULL_BUILD_DEPENDS
    libc.include.stdio
    libc.include.llvm-libc-macros.stdio_macros
    libc.include.llvm-libc-macros.file_seek_macros
)

add_proxy_header_library(
  sys_epoll_macros
  HDRS
    sys_epoll_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_epoll
    libc.include.llvm-libc-macros.sys_epoll_macros
)

add_proxy_header_library(
  sys_ioctl_macros
  HDRS
    sys_ioctl_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_ioctl
    libc.include.llvm-libc-macros.sys_ioctl_macros
)

add_proxy_header_library(
  sys_ipc_macros
  HDRS
    sys_ipc_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_ipc
    libc.include.llvm-libc-macros.sys_ipc_macros
)

add_proxy_header_library(
  sys_sem_macros
  HDRS
    sys_sem_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_sem
    libc.include.llvm-libc-macros.sys_sem_macros
)

add_proxy_header_library(
  sys_socket_macros
  HDRS
    sys_socket_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_socket
    libc.include.llvm-libc-macros.sys_socket_macros
)

add_proxy_header_library(
  sys_stat_macros
  HDRS
    sys_stat_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_stat
    libc.include.llvm-libc-macros.sys_stat_macros
)

add_proxy_header_library(
  sys_mman_macros
  HDRS
    sys_mman_macros.h
  FULL_BUILD_DEPENDS
    libc.include.sys_mman
    libc.include.llvm-libc-macros.sys_mman_macros
)

add_header_library(unistd_overlay HDRS unistd_overlay.h)
add_proxy_header_library(
  unistd_macros
  HDRS
    unistd_macros.h
  DEPENDS
    .unistd_overlay
  FULL_BUILD_DEPENDS
    libc.include.unistd
    libc.include.llvm-libc-macros.unistd_macros
)

if (WIN32)
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
    libc.include.time
    libc.include.llvm-libc-macros.time_macros
)

add_proxy_header_library(
  float_macros
  HDRS
    float_macros.h
  DEPENDS
    libc.include.llvm-libc-macros.float_macros
  FULL_BUILD_DEPENDS
    libc.include.float
)

add_proxy_header_library(
  limits_macros
  HDRS
    limits_macros.h
  FULL_BUILD_DEPENDS
    libc.include.limits
    libc.include.llvm-libc-macros.limits_macros
)

add_proxy_header_library(
  link_macros
  HDRS
    link_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.link_macros
    libc.include.link
)

add_proxy_header_library(
  locale_macros
  HDRS
    locale_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.locale_macros
    libc.include.locale
)

add_proxy_header_library(
  sys_auxv_macros
  HDRS
    sys_auxv_macros.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-macros.sys_auxv_macros
    libc.include.sys_auxv
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
    libc.include.wchar
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
    libc.include.elf
)

add_gen_header(
  elf_proxy
  YAML_FILE
    ../libc/include/elf.yaml
  GEN_HDR
    elf_proxy.h
  DEPENDS
    libc.include.llvm_libc_common_h
    libc.include.llvm-libc-types.Elf32_Addr
    libc.include.llvm-libc-types.Elf32_Chdr
    libc.include.llvm-libc-types.Elf32_Dyn
    libc.include.llvm-libc-types.Elf32_Ehdr
    libc.include.llvm-libc-types.Elf32_Half
    libc.include.llvm-libc-types.Elf32_Lword
    libc.include.llvm-libc-types.Elf32_Nhdr
    libc.include.llvm-libc-types.Elf32_Off
    libc.include.llvm-libc-types.Elf32_Phdr
    libc.include.llvm-libc-types.Elf32_Rel
    libc.include.llvm-libc-types.Elf32_Rela
    libc.include.llvm-libc-types.Elf32_Shdr
    libc.include.llvm-libc-types.Elf32_Sword
    libc.include.llvm-libc-types.Elf32_Sym
    libc.include.llvm-libc-types.Elf32_Verdaux
    libc.include.llvm-libc-types.Elf32_Verdef
    libc.include.llvm-libc-types.Elf32_Vernaux
    libc.include.llvm-libc-types.Elf32_Verneed
    libc.include.llvm-libc-types.Elf32_Versym
    libc.include.llvm-libc-types.Elf32_Word
    libc.include.llvm-libc-types.Elf32_Xword
    libc.include.llvm-libc-types.Elf32_auxv_t
    libc.include.llvm-libc-types.Elf64_Addr
    libc.include.llvm-libc-types.Elf64_Chdr
    libc.include.llvm-libc-types.Elf64_Dyn
    libc.include.llvm-libc-types.Elf64_Ehdr
    libc.include.llvm-libc-types.Elf64_Half
    libc.include.llvm-libc-types.Elf64_Lword
    libc.include.llvm-libc-types.Elf64_Nhdr
    libc.include.llvm-libc-types.Elf64_Off
    libc.include.llvm-libc-types.Elf64_Phdr
    libc.include.llvm-libc-types.Elf64_Rel
    libc.include.llvm-libc-types.Elf64_Rela
    libc.include.llvm-libc-types.Elf64_Shdr
    libc.include.llvm-libc-types.Elf64_Sword
    libc.include.llvm-libc-types.Elf64_Sxword
    libc.include.llvm-libc-types.Elf64_Sym
    libc.include.llvm-libc-types.Elf64_Verdaux
    libc.include.llvm-libc-types.Elf64_Verdef
    libc.include.llvm-libc-types.Elf64_Vernaux
    libc.include.llvm-libc-types.Elf64_Verneed
    libc.include.llvm-libc-types.Elf64_Versym
    libc.include.llvm-libc-types.Elf64_Word
    libc.include.llvm-libc-types.Elf64_Xword
    libc.include.llvm-libc-types.Elf64_auxv_t
  PROXY
)

add_subdirectory(types)
add_subdirectory(func)
