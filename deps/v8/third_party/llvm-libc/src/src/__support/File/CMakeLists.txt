add_header_library(
  file_io_result
  HDRS
    file_io_result.h
  DEPENDS
    libc.src.__support.macros.config
)

if(NOT (TARGET libc.src.__support.threads.mutex)
  OR LIBC_TARGET_OS_IS_GPU)
  # Not all platforms have a mutex implementation. If mutex is unvailable,
  # we just skip everything about files.
  return()
endif()

add_object_library(
  file
  SRCS
    file.cpp
  HDRS
    file.h
  DEPENDS
    .file_io_result
    libc.hdr.stdio_macros
    libc.hdr.stdint_proxy
    libc.hdr.func.realloc
    libc.hdr.types.off_t
    libc.hdr.types.wchar_t
    libc.hdr.types.wint_t
    libc.hdr.wchar_macros
    libc.src.__support.CPP.new
    libc.src.__support.CPP.span
    libc.src.__support.threads.mutex
    libc.src.__support.error_or
    libc.src.__support.macros.config
    libc.src.__support.macros.properties.architectures
    libc.src.__support.wchar.mbstate
    libc.src.__support.wchar.wcrtomb
    libc.src.__support.wchar.character_converter
    libc.src.string.memory_utils.inline_memcpy
    libc.hdr.errno_macros
)

add_header_library(
  dir_scan_impl
  HDRS
    dir_scan_impl.h
  DEPENDS
    libc.src.__support.error_or
    libc.src.__support.CPP.limits
    libc.src.stdlib.qsort_util
    libc.src.string.memory_utils.inline_memcpy
    libc.hdr.errno_macros
    libc.hdr.types.struct_dirent
)

add_object_library(
  dir
  SRCS
    dir.cpp
  HDRS
    dir.h
  DEPENDS
    .dir_scan_impl
    libc.src.__support.CPP.mutex
    libc.src.__support.CPP.new
    libc.src.__support.CPP.span
    libc.src.__support.threads.mutex
    libc.src.__support.error_or
    libc.src.__support.macros.config
    libc.src.__support.alloc_checker
    libc.hdr.errno_macros
    libc.hdr.types.struct_dirent
)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  return()
endif()

add_subdirectory(${LIBC_TARGET_OS})

set(target_file libc.src.__support.File.${LIBC_TARGET_OS}.file)

add_object_library(
  platform_file
  ALIAS
    ${target_file}
  DEPENDS
    ${target_file}
)

set(target_dir libc.src.__support.File.${LIBC_TARGET_OS}.${LIBC_TARGET_OS}_dir)
if(NOT TARGET ${target_dir})
  return()
endif()

add_object_library(
  platform_dir
  ALIAS
    ${target_dir}
  DEPENDS
    ${target_dir}
)
