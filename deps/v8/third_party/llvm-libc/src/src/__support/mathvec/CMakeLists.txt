add_header_library(common_constants HDRS common_constants.h)

add_header_library(
  acosf
  HDRS
    acosf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.acosf
)

add_header_library(
  acoshf
  HDRS
    acoshf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.acoshf
)

add_header_library(
  acospif
  HDRS
    acospif.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.acospif
)

add_header_library(
  asinf
  HDRS
    asinf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.asinf
)

add_header_library(
  asinhf
  HDRS
    asinhf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.asinhf
)

add_header_library(
  asinpif
  HDRS
    asinpif.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.asinpif
)

add_header_library(
  atanf
  HDRS
    atanf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.atanf
)

add_header_library(
  atanhf
  HDRS
    atanhf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.atanhf
)

add_header_library(
  cbrtf
  HDRS
    cbrtf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.cbrtf
)

add_header_library(
  cosf
  HDRS
    cosf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.cosf
)

add_header_library(
  coshf
  HDRS
    coshf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.coshf
)

add_header_library(
  cospif
  HDRS
    cospif.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.cospif
)

add_header_library(
  erff
  HDRS
    erff.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.erff
)

add_header_library(
  exp2f
  HDRS
    exp2f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.exp2f
)

add_header_library(
  exp2m1f
  HDRS
    exp2m1f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.exp2m1f
)

add_header_library(
  exp10f
  HDRS
    exp10f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.exp10f
)

add_header_library(
  exp10m1f
  HDRS
    exp10m1f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.exp10m1f
)

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


add_header_library(
  expm1f
  HDRS
    expm1f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.expm1f
)

add_header_library(
  log10f
  HDRS
    log10f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.log10f
)

add_header_library(
  log1pf
  HDRS
    log1pf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.log1pf
)

add_header_library(
  log2f
  HDRS
    log2f.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.log2f
)

add_header_library(
  logf
  HDRS
    logf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.logf
)

add_header_library(
  rsqrtf
  HDRS
    rsqrtf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.rsqrtf
)

add_header_library(
  sinf
  HDRS
    sinf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.sinf
)

add_header_library(
  sinhf
  HDRS
    sinhf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.sinhf
)

add_header_library(
  sinpif
  HDRS
    sinpif.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.sinpif
)

add_header_library(
  sqrtf
  HDRS
    sqrtf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.sqrtf
)

add_header_library(
  tanf
  HDRS
    tanf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.tanf
)

add_header_library(
  tanhf
  HDRS
    tanhf.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.tanhf
)

add_header_library(
  tanpif
  HDRS
    tanpif.h
  DEPENDS
    libc.src.__support.CPP.simd
    libc.src.__support.math.tanpif
)