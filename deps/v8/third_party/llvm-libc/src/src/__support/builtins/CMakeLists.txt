add_header_library(
  addtf3
  HDRS
    addtf3.h
  DEPENDS
    libc.include.llvm-libc-types.float128
    libc.src.__support.FPUtil.generic.add_sub
    libc.src.__support.macros.config
)

add_header_library(
  subtf3
  HDRS
    subtf3.h
  DEPENDS
    libc.include.llvm-libc-types.float128
    libc.src.__support.FPUtil.generic.add_sub
    libc.src.__support.macros.config
)

add_header_library(
  multf3
  HDRS
    multf3.h
  DEPENDS
    libc.include.llvm-libc-types.float128
    libc.src.__support.FPUtil.generic.mul
    libc.src.__support.macros.config
)

add_header_library(
  divtf3
  HDRS
    divtf3.h
  DEPENDS
    libc.include.llvm-libc-types.float128
    libc.src.__support.FPUtil.generic.div
    libc.src.__support.macros.config
)

add_header_library(
  adddf3
  HDRS
    adddf3.h
  DEPENDS
    libc.src.__support.FPUtil.generic.add_sub
    libc.src.__support.macros.config
)

add_header_library(
  subdf3
  HDRS
    subdf3.h
  DEPENDS
    libc.src.__support.FPUtil.generic.add_sub
    libc.src.__support.macros.config
)

add_header_library(
  muldf3
  HDRS
    muldf3.h
  DEPENDS
    libc.src.__support.FPUtil.generic.mul
    libc.src.__support.macros.config
)
