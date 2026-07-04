add_header_library(common_constants HDRS common_constants.h)

add_header_library(
  expf_utils
  HDRS
    expf_utils.h
  DEPENDS
    .common_constants
    libc.src.__support.CPP.simd
)

add_header_library(
  expf
  HDRS
    expf.h
  DEPENDS
    .expf_utils
    libc.src.__support.CPP.simd
    libc.src.__support.FPUtil.fp_bits
    libc.src.__support.common
)
