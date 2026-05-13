add_header_library(
  stdc_errors
  HDRS
    stdc_errors.h
  DEPENDS
    libc.hdr.errno_macros
    libc.src.__support.StringUtil.message_mapper
)

add_header_library(
  posix_errors
  HDRS
    posix_errors.h
  DEPENDS
    libc.src.errno.errno
    libc.src.__support.StringUtil.message_mapper
)

add_header_library(
  linux_extension_errors
  HDRS
    linux_extension_errors.h
  DEPENDS
    libc.src.__support.StringUtil.message_mapper
    libc.src.errno.errno
)

add_header_library(
  linux_platform_errors
  HDRS
    linux_platform_errors.h
  DEPENDS
    .linux_extension_errors
    .posix_errors
    .stdc_errors
)

add_header_library(
  minimal_platform_errors
  HDRS
    minimal_platform_errors.h
  DEPENDS
    .stdc_errors
)

add_header_library(
  stdc_signals
  HDRS
    stdc_signals.h
  DEPENDS
    libc.include.signal
    libc.src.__support.StringUtil.message_mapper
)

add_header_library(
  posix_signals
  HDRS
    posix_signals.h
  DEPENDS
    libc.include.signal
    libc.src.__support.StringUtil.message_mapper
)

add_header_library(
  linux_extension_signals
  HDRS
    linux_extension_signals.h
  DEPENDS
    libc.include.signal
    libc.src.__support.StringUtil.message_mapper
)

add_header_library(
  linux_platform_signals
  HDRS
    linux_platform_signals.h
  DEPENDS
    .linux_extension_signals
    .posix_signals
    .stdc_signals
)

add_header_library(
  minimal_platform_signals
  HDRS
    minimal_platform_signals.h
  DEPENDS
    .stdc_signals
)
