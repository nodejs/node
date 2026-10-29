# These utilities are GPU only.
if(NOT LIBC_TARGET_OS_IS_GPU)
  return()
endif()

add_header_library(
  utils
  HDRS
    utils.h
)

add_header_library(
  fixedbuffer
  HDRS
    fixedbuffer.h
)

add_object_library(
  allocator
  SRCS
    allocator.cpp
  HDRS
    allocator.h
  DEPENDS
    libc.hdr.stdint_proxy
    libc.src.__support.common
    libc.src.__support.RPC.rpc_client
    libc.src.__support.CPP.atomic
    libc.src.__support.CPP.bit
    libc.src.__support.CPP.new
    .utils
    .fixedbuffer
)
