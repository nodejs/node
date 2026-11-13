add_header(
  loff_t
  HDR
    loff_t.h
)

add_header(
  struct_sysinfo
  HDR
    struct_sysinfo.h
)

add_header(
  struct_ifmap
  HDR
    struct_ifmap.h
)

add_header(
  struct_ifreq
  HDR
    struct_ifreq.h
  DEPENDS
    .struct_ifmap
    libc.include.llvm-libc-macros.net_if_macros
    libc.include.llvm-libc-types.struct_sockaddr
)
