add_subdirectory(properties)

add_header_library(
  config
  HDRS
    config.h
  DEPENDS
    libc.src.__support.macros.properties.architectures
    libc.src.__support.macros.properties.compiler
)

add_header_library(
  attributes
  HDRS
    attributes.h
)

add_header_library(
  optimization
  HDRS
    optimization.h
  DEPENDS
    libc.src.__support.macros.properties.compiler
)

add_header_library(
  sanitizer
  HDRS
    sanitizer.h
  DEPENDS
    libc.src.__support.macros.properties.compiler
)

add_header_library(
  null_check
  HDRS
    null_check.h
  DEPENDS
    .config
    .optimization
)

add_header_library(
  macro_utils
  HDRS
    macro-utils.h
)

add_header_library(
  is_defined
  HDRS
    is_defined.h
  DEPENDS
    .macro_utils
    libc.src.__support.CPP.string_view
)
