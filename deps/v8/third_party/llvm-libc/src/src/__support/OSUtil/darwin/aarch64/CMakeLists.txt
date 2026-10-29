add_header_library(
  darwin_aarch64_util
  HDRS
    syscall.h
  DEPENDS
    libc.src.__support.common
)
