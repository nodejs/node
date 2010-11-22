if(SHARED_CARES)
  find_library(LIBCARES_LIBRARY NAMES cares)
  find_path(LIBCARES_INCLUDE_DIR ares.h
    PATH_SUFFIXES include
    ) # Find header
  find_package_handle_standard_args(libcares DEFAULT_MSG LIBCARES_LIBRARY LIBCARES_INCLUDE_DIR)
else()
  set(cares_arch ${node_arch})

  if(${node_arch} MATCHES x86_64)
    set(cares_arch x64)
  elseif(${node_arch} MATCHES x86)
    set(cares_arch ia32)
  endif()

  add_subdirectory(deps/c-ares)
  set(LIBCARES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/deps/c-ares ${CMAKE_SOURCE_DIR}/deps/c-ares/${node_platform}-${cares_arch})
endif()
