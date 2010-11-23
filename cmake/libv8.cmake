set(V8_INCLUDE_NAMES v8.h v8-debug.h v8-profiler.h v8stdint.h)
set(V8_LIBRARY_NAMES v8)

if(SHARED_V8)
  find_path(V8_INCLUDE_DIR NAMES ${V8_INCLUDE_NAMES})
  find_library(V8_LIBRARY_PATH NAMES ${V8_LIBRARY_NAMES} NO_CMAKE_PATH)
else()
  set(V8_INCLUDE_DIR "${PROJECT_BINARY_DIR}/deps/v8/include")
  if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    set(v8_fn "libv8_g.a")
  else()
    set(v8_fn "libv8.a")
  endif()
  set(V8_LIBRARY_PATH "${PROJECT_BINARY_DIR}/deps/v8/${v8_fn}")
  install(DIRECTORY
      ## Do NOT remove the trailing slash
      ## it is required so that v8 headers are
      ## copied directly into include/node
      ## rather than in a subdirectory
      ## See CMake's install(DIRECTORY) manual for details
      ${V8_INCLUDE_DIR}/

      DESTINATION include/node
  )
endif()
