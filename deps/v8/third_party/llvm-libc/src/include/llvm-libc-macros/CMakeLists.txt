# Helper function to set up dependencies if they exist.
function(add_macro_header name)
  cmake_parse_arguments(
    "MACRO_HEADER"
    "" # Optional arguments
    "HDR;DEST_HDR" # Single value arguments
    "DEPENDS" # Multi-value arguments
    ${ARGN}
  )
  if (MACRO_HEADER_DEST_HDR)
    set(dest_header_arg DEST_HDR ${MACRO_HEADER_DEST_HDR})
  endif()
  if(TARGET libc.include.llvm-libc-macros.${LIBC_TARGET_OS}.${name})
    add_header(
      ${name}
      HDR
        ${MACRO_HEADER_HDR}
      ${dest_header_arg}
      DEPENDS
        .${LIBC_TARGET_OS}.${name}
        ${MACRO_HEADER_DEPENDS}
    )
  else()
    add_header(
      ${name}
      HDR
        ${MACRO_HEADER_HDR}
      ${dest_header_arg}
      DEPENDS
        ${MACRO_HEADER_DEPENDS}
    )
  endif()
endfunction(add_macro_header)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
endif()

add_macro_header(
  annex_k_macros
  HDR
    annex-k-macros.h
)

add_macro_header(
  assert_macros
  HDR
    assert-macros.h
)

add_macro_header(
  byteswap_macros
  HDR
    byteswap-macros.h
)

add_macro_header(
  error_number_macros
  HDR
    error-number-macros.h
)

add_macro_header(
  generic_error_number_macros
  HDR
    generic-error-number-macros.h
)

add_macro_header(
  null_macro
  HDR
    null-macro.h
)

add_macro_header(
  fcntl_macros
  HDR
    fcntl-macros.h
)

add_macro_header(
  cfloat128_macros
  HDR
    cfloat128-macros.h
)

add_macro_header(
  cfloat16_macros
  HDR
    cfloat16-macros.h
)

add_macro_header(
  complex_macros
  HDR
    complex-macros.h
  DEPENDS
    .cfloat128_macros
    .cfloat16_macros
)

add_macro_header(
  features_macros
  HDR
    features-macros.h
)

add_macro_header(
  fenv_macros
  HDR
    fenv-macros.h
)

add_macro_header(
  file_seek_macros
  HDR
    file-seek-macros.h
)

add_macro_header(
  stdint_macros
  HDR
    stdint-macros.h
)

add_macro_header(
  float_macros
  HDR
    float-macros.h
)

add_macro_header(
  float16_macros
  HDR
    float16-macros.h
)

add_macro_header(
  limits_macros
  HDR
    limits-macros.h
)

add_macro_header(
  link_macros
  HDR
    link-macros.h
)

add_macro_header(
  malloc_macros
  HDR
    malloc-macros.h
)

add_macro_header(
  math_macros
  HDR
    math-macros.h
  DEPENDS
    .limits_macros
)

add_macro_header(
  math_function_macros
  HDR
    math-function-macros.h
  DEPENDS
    .math_macros
)

add_macro_header(
  netinet_in_macros
  HDR
    netinet-in-macros.h
)

add_macro_header(
  offsetof_macro
  HDR
    offsetof-macro.h
)

add_macro_header(
  containerof_macro
  HDR
    containerof-macro.h
  DEPENDS
    .offsetof_macro
)

add_macro_header(
  sched_macros
  HDR
    sched-macros.h
)

add_macro_header(
  signal_macros
  HDR
    signal-macros.h
)

add_macro_header(
  stdbit_macros
  HDR
    stdbit-macros.h
)

add_macro_header(
  stdio_macros
  HDR
    stdio-macros.h
)

add_macro_header(
  stdlib_macros
  HDR
    stdlib-macros.h
)

add_macro_header(
  sys_auxv_macros
  HDR
    sys-auxv-macros.h
)

add_macro_header(
  sys_epoll_macros
  HDR
    sys-epoll-macros.h
)

add_macro_header(
  sys_ioctl_macros
  HDR
    sys-ioctl-macros.h
)

add_macro_header(
  sys_ipc_macros
  HDR
    sys-ipc-macros.h
)

add_macro_header(
  sys_sem_macros
  HDR
    sys-sem-macros.h
)

add_macro_header(
  sys_stat_macros
  HDR
    sys-stat-macros.h
)

add_macro_header(
  sys_mman_macros
  HDR
    sys-mman-macros.h
)

add_macro_header(
  sys_personality_macros
  HDR
    sys-personality-macros.h
)

add_macro_header(
  sys_queue_macros
  HDR
    sys-queue-macros.h
  DEPENDS
    .null_macro
    .containerof_macro
)

add_macro_header(
  sys_random_macros
  HDR
    sys-random-macros.h
)

add_macro_header(
  sys_resource_macros
  HDR
    sys-resource-macros.h
)

add_macro_header(
  sys_select_macros
  HDR
    sys-select-macros.h
)

add_macro_header(
  sys_socket_macros
  HDR
    sys-socket-macros.h
)

add_macro_header(
  sys_time_macros
  HDR
    sys-time-macros.h
)

add_macro_header(
  sys_wait_macros
  HDR
    sys-wait-macros.h
)

add_macro_header(
  termios_macros
  HDR
    termios-macros.h
)

add_macro_header(
  time_macros
  HDR
    time-macros.h
)

add_macro_header(
  unistd_macros
  HDR
    unistd-macros.h
)

add_macro_header(
  wchar_macros
  HDR
    wchar-macros.h
)

add_macro_header(
  inttypes_macros
  HDR
    inttypes-macros.h
)

add_macro_header(
  stdfix_macros
  HDR
    stdfix-macros.h
)

add_macro_header(
  stdckdint_macros
  HDR
    stdckdint-macros.h
)

add_macro_header(
  dlfcn_macros
  HDR
    dlfcn-macros.h
)

add_macro_header(
  elf_macros
  HDR
    elf-macros.h
)

add_macro_header(
  endian_macros
  HDR
    endian-macros.h
  DEPENDS
    .stdint_macros
)

add_macro_header(
  locale_macros
  HDR
    locale-macros.h
)

add_macro_header(
  nl_types_macros
  HDR
    nl_types-macros.h
)

add_macro_header(
  pthread_macros
  HDR
    pthread-macros.h
  DEPENDS
    .null_macro
)

add_macro_header(
  poll-macros
  HDR
    poll-macros.h
)

add_macro_header(
  EFIAPI_macros
  HDR
    EFIAPI-macros.h
)

add_macro_header(
  sysexits_macros
  HDR
    sysexits-macros.h
)

if (LIBC_CONF_MODULAR_FORMAT)
  add_macro_header(
    _LIBC_MODULAR_FORMAT_PRINTF
    HDR
      _LIBC_MODULAR_FORMAT_PRINTF.h
  )
else()
  add_macro_header(
    _LIBC_MODULAR_FORMAT_PRINTF
    HDR
      _LIBC_MODULAR_FORMAT_PRINTF-disable.h
      DEST_HDR
      _LIBC_MODULAR_FORMAT_PRINTF.h
  )
endif()
