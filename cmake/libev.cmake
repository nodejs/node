if(SHARED_LIBEV)
  find_library(LIBEV_LIBRARY NAMES ev)
  find_path(LIBEV_INCLUDE_DIR ev.h
    PATH_SUFFIXES include/ev include
    ) # Find header
  find_package_handle_standard_args(libev DEFAULT_MSG LIBEV_LIBRARY LIBEV_INCLUDE_DIR)
else()
  add_subdirectory(deps/libev)
  set(LIBEV_INCLUDE_DIR deps/libev)
endif()
