add_header_library(
  error_mapper
  HDRS
    error_mapper.h
  DEPENDS
    libc.src.__support.printf_core.core_structs
    libc.hdr.errno_macros
)
