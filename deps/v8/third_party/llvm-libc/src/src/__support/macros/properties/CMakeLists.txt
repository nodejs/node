add_header_library(
  architectures
  HDRS
    architectures.h
)

add_header_library(
  compiler
  HDRS
    compiler.h
)

add_header_library(
  os
  HDRS
    os.h
)

add_header_library(
  cpu_features
  HDRS
    cpu_features.h
  DEPENDS
    .architectures
)

add_header_library(
  types
  HDRS
    types.h
  DEPENDS
    .architectures
    .compiler
    .cpu_features
    .os
    libc.hdr.float_macros
    libc.hdr.stdint_proxy
    libc.include.llvm-libc-macros.float16_macros
    libc.include.llvm-libc-types.float128
)

add_header_library(
  complex_types
  HDRS
    complex_types.h
  DEPENDS
    .types
    libc.include.llvm-libc-types.cfloat16
    libc.include.llvm-libc-types.cfloat128
)
