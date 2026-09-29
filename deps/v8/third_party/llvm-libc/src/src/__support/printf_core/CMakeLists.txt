if(NOT LLVM_LIBC_FULL_BUILD)
  set(use_system_file "COMPILE_OPTIONS")
  libc_add_definition(use_system_file "LIBC_COPT_STDIO_USE_SYSTEM_FILE")
endif()

if(LIBC_CONF_PRINTF_DISABLE_FLOAT)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_DISABLE_FLOAT")
endif()
if(LIBC_CONF_PRINTF_DISABLE_INDEX_MODE)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_DISABLE_INDEX_MODE")
endif()
if(LIBC_CONF_PRINTF_DISABLE_WRITE_INT)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_DISABLE_WRITE_INT")
endif()
if(LIBC_CONF_PRINTF_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE)
  libc_add_definition(printf_config_copts "LIBC_COPT_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE")
endif()
if(LIBC_CONF_PRINTF_FLOAT_TO_STR_USE_DYADIC_FLOAT)
  libc_add_definition(printf_config_copts "LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT")
endif()
if(LIBC_CONF_PRINTF_FLOAT_TO_STR_NO_SPECIALIZE_LD)
  libc_add_definition(printf_config_copts "LIBC_COPT_FLOAT_TO_STR_NO_SPECIALIZE_LD")
endif()
if(LIBC_CONF_PRINTF_FLOAT_TO_STR_USE_FLOAT320)
  libc_add_definition(printf_config_copts "LIBC_COPT_FLOAT_TO_STR_USE_FLOAT320")
endif()
if(LIBC_CONF_PRINTF_DISABLE_FIXED_POINT)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_DISABLE_FIXED_POINT")
endif()
if(LIBC_CONF_PRINTF_DISABLE_STRERROR)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_DISABLE_STRERROR")
endif()
if(LIBC_CONF_PRINTF_RUNTIME_DISPATCH)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_RUNTIME_DISPATCH")
endif()
if(LIBC_CONF_PRINTF_MODULAR)
  libc_add_definition(printf_config_copts "LIBC_COPT_PRINTF_MODULAR")
endif()
if(printf_config_copts)
  list(PREPEND printf_config_copts "COMPILE_OPTIONS")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  add_subdirectory(${LIBC_TARGET_OS})
else()
  add_subdirectory(generic)
endif()

set(target_error_mapper libc.src.__support.printf_core.${LIBC_TARGET_OS}.error_mapper)
if(NOT TARGET ${target_error_mapper})
    set(target_error_mapper libc.src.__support.printf_core.generic.error_mapper)
endif()

if(LIBC_CONF_PRINTF_DISABLE_WIDE)
  set(wchar_deps "")
  set(parser_wchar_deps "")
else()
  set(wchar_deps
    libc.hdr.types.wchar_t
    libc.hdr.types.char32_t
    libc.hdr.types.char8_t
    libc.hdr.types.wint_t
    libc.hdr.wchar_macros
    libc.src.__support.wchar.wcrtomb
    libc.src.__support.wchar.mbstate
    libc.src.__support.wchar.string_converter
  )
  set(parser_wchar_deps
    libc.hdr.types.wint_t
  )
endif()

add_header_library(
  printf_config
  HDRS
    printf_config.h
  ${printf_config_copts}
)

add_header_library(
  core_structs
  HDRS
    core_structs.h
  DEPENDS
    libc.include.inttypes
    libc.src.__support.CPP.string_view
    libc.src.__support.FPUtil.fp_bits
    libc.hdr.errno_macros
)

add_header_library(
  parser
  HDRS
    parser.h
  DEPENDS
    .core_structs
    libc.src.__support.arg_list
    libc.src.__support.ctype_utils
    libc.src.__support.str_to_integer
    libc.src.__support.CPP.algorithm
    libc.src.__support.CPP.bit
    libc.src.__support.CPP.optional
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.type_traits
    libc.src.__support.common
    ${parser_wchar_deps}
)

add_header_library(
  writer
  HDRS
    writer.h
  DEPENDS
    .core_structs
    libc.src.__support.CPP.string_view
    libc.src.__support.macros.optimization
    libc.src.string.memory_utils.inline_memcpy
    libc.src.string.memory_utils.inline_memset
)

add_header_library(
  converter
  HDRS
    converter.h
    converter_atlas.h
    converter_utils.h
    string_converter.h
    char_converter.h
    int_converter.h
    ptr_converter.h
    write_int_converter.h
    float_inf_nan_converter.h
    float_hex_converter.h
    float_dec_converter.h
    fixed_converter.h #TODO: Check if this should be disabled when fixed unavail
    strerror_converter.h
  DEPENDS
    .core_structs
    .printf_config
    .writer
    libc.include.inttypes
    libc.hdr.limits_macros
    libc.src.string.string_utils
    libc.src.__support.big_int
    libc.src.__support.common
    libc.src.__support.CPP.limits
    libc.src.__support.CPP.span
    libc.src.__support.CPP.string_view
    libc.src.__support.float_to_string
    libc.src.__support.FPUtil.fenv_impl
    libc.src.__support.FPUtil.fp_bits
    libc.src.__support.FPUtil.rounding_mode
    libc.src.__support.integer_to_string
    libc.src.__support.libc_assert
    libc.src.__support.uint128
    libc.src.__support.StringUtil.error_to_string
    libc.src.string.memory_utils.inline_memcpy
    ${wchar_deps}
)

add_object_library(
  printf_main
  HDRS
    printf_main.h
  SRCS
    float_impl.cpp
  DEPENDS
    .parser
    .converter
    .writer
    .core_structs
    libc.src.__support.arg_list
    libc.src.__support.error_or
)

add_header_library(
  vasprintf_internal
  HDRS
    vasprintf_internal.h
  DEPENDS
    libc.hdr.func.malloc
    libc.hdr.func.free
    libc.hdr.func.realloc
    libc.src.__support.arg_list
    libc.src.__support.error_or
    libc.src.__support.printf_core.printf_main
    libc.src.__support.printf_core.writer
)

add_header_library(
  error_mapper
  HDRS
    error_mapper.h
  DEPENDS
    ${target_error_mapper}
    libc.src.__support.macros.properties.architectures
)

if(NOT (TARGET libc.src.__support.File.file) AND LLVM_LIBC_FULL_BUILD)
  # Not all platforms have a file implementation. If file is unvailable, and a
  # full build is requested, then we must skip all file based printf sections.
  return()
endif()

add_header_library(
  vfprintf_internal
  HDRS
    vfprintf_internal.h
  DEPENDS
    libc.src.__support.File.file
    libc.src.__support.error_or
    libc.src.__support.arg_list
    libc.src.__support.printf_core.printf_main
    libc.src.__support.printf_core.writer
  ${use_system_file}
)

