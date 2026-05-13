add_header_library(
  bitmask
  HDRS
    bitmask.h
  FLAGS
    EXPLICIT_SIMD_OPT
  DEPENDS
    libc.hdr.stdint_proxy
    libc.src.__support.common
    libc.src.__support.CPP.bit
    libc.src.__support.macros.properties.cpu_features
)

list(FIND TARGET_ENTRYPOINT_NAME_LIST getrandom getrandom_index)
if (NOT ${getrandom_index} EQUAL -1)
  message(STATUS "Using getrandom for hashtable randomness")
  libc_set_definition(randomness_compile_flags LIBC_HASHTABLE_USE_GETRANDOM)
  set(randomness_extra_depends
    libc.src.__support.OSUtil.linux.syscall_wrappers.getrandom
    libc.hdr.errno_macros)
endif()


add_header_library(
  table
  HDRS
    table.h
  DEPENDS
    .bitmask
    libc.hdr.stdint_proxy
    libc.hdr.types.ENTRY
    libc.src.__support.CPP.bit
    libc.src.__support.CPP.new
    libc.src.__support.hash
    libc.src.__support.macros.attributes
    libc.src.__support.macros.optimization
    libc.src.__support.memory_size
    libc.src.string.memory_utils.inline_strcmp
    libc.src.string.string_utils
)

add_header_library(
  randomness
  HDRS
    randomness.h
  COMPILE_OPTIONS
    ${randomness_compile_flags}
  DEPENDS
    libc.src.__support.hash
    libc.src.__support.common
    ${randomness_extra_depends}
)
