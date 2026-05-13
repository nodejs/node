add_header_library(
  message_mapper
  HDRS
    message_mapper.h
  DEPENDS
    libc.src.__support.CPP.array
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.optional
)

# The table maps depend on message_mapper.
add_subdirectory(tables)

add_header_library(
  platform_errors
  HDRS
    platform_errors.h
  DEPENDS
    # To avoid complicated conditionals, we will unconditionally add dependency
    # on all platform errors which are included in platform_error_table.h.
    # Ultimately, only the relevent error table will be used.
    .tables.linux_platform_errors
    .tables.minimal_platform_errors
)

add_header_library(
  platform_signals
  HDRS
    platform_signals.h
  DEPENDS
    # To avoid complicated conditionals, we will unconditionally add dependency
    # on all platform signals which are included in platform_signal_table.h.
    # Ultimately, only the relevent signal table will be used.
    .tables.linux_platform_signals
    .tables.minimal_platform_signals
)

add_object_library(
  error_to_string
  HDRS
    error_to_string.h
  SRCS
    error_to_string.cpp
  DEPENDS
    .message_mapper
    .platform_errors
    libc.src.__support.common
    libc.src.__support.CPP.span
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.stringstream
    libc.src.__support.integer_to_string
)

if(TARGET libc.include.signal)
  add_object_library(
    signal_to_string
    HDRS
      signal_to_string.h
    SRCS
      signal_to_string.cpp
    DEPENDS
      .message_mapper
      .platform_signals
      libc.include.signal
      libc.src.__support.common
      libc.src.__support.CPP.span
      libc.src.__support.CPP.string_view
      libc.src.__support.CPP.stringstream
      libc.src.__support.integer_to_string
  )
endif()
