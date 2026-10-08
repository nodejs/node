add_header_library(
  linux_aarch64_util
  HDRS
    syscall.h
  DEPENDS
    libc.src.__support.common
)

add_header_library(
  vdso
  HDRS
    vdso.h
  DEPENDS
    libc.src.__support.common
    libc.src.__support.CPP.string_view
    libc.src.__support.OSUtil.linux.vdso_sym
)
