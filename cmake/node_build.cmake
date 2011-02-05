#
# node build stuff
#

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/src/node_natives.h
  COMMAND ${PYTHON_EXECUTABLE} tools/js2c.py ${PROJECT_BINARY_DIR}/src/node_natives.h ${js2c_files}
  DEPENDS ${js2c_files})

set(node_platform_src "src/platform_${node_platform}.cc")

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/${node_platform_src})
  set(node_extra_src ${node_extra_src} "src/platform_none.cc")
else()
  set(node_extra_src ${node_extra_src} ${node_platform_src})
endif()

set(node_sources
  src/node_main.cc
  src/node.cc
  src/node_buffer.cc
  src/node_javascript.cc
  src/node_extensions.cc
  src/node_http_parser.cc
  src/node_net.cc
  src/node_io_watcher.cc
  src/node_child_process.cc
  src/node_constants.cc
  src/node_cares.cc
  src/node_events.cc
  src/node_file.cc
  src/node_signal_watcher.cc
  src/node_stat_watcher.cc
  src/node_stdio.cc
  src/node_timer.cc
  src/node_script.cc
  src/node_os.cc
  src/node_dtrace.cc
  src/node_natives.h
  ${node_extra_src})

# Set up PREFIX, CCFLAGS, and CPPFLAGS for node_config.h
set(PREFIX ${CMAKE_INSTALL_PREFIX})
if(${CMAKE_BUILD_TYPE} MATCHES Debug)
  set(CCFLAGS "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS}")
else()
  set(CCFLAGS "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS}")
endif()
get_directory_property(compile_defs COMPILE_DEFINITIONS)
foreach(def ${compile_defs})
  # escape " in CPPFLAGS (-DPLATFORM="${node_platform}" would fuck stuff up
  # otherwise)
  string(REPLACE "\"" "\\\"" def ${def})
  set(CPPFLAGS "${CPPFLAGS} -D${def}")
endforeach()

configure_file(src/node_config.h.in ${PROJECT_BINARY_DIR}/src/node_config.h)
configure_file(config.h.cmake ${PROJECT_BINARY_DIR}/config.h)

include_directories(
  src
  deps/libeio
  deps/http_parser
  ${V8_INCLUDE_DIR}
  ${LIBEV_INCLUDE_DIR}
  ${LIBCARES_INCLUDE_DIR}

  ${PROJECT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}/src
)

add_executable(node ${node_sources})
set_target_properties(node PROPERTIES DEBUG_POSTFIX "_g")
target_link_libraries(node
  ev
  eio
  cares
  http_parser
  ${V8_LIBRARY_PATH}
  ${CMAKE_THREAD_LIBS_INIT}
  ${extra_libs})


install(TARGETS node RUNTIME DESTINATION bin)
install(FILES     
  ${PROJECT_BINARY_DIR}/config.h
  src/node.h
  src/node_object_wrap.h
  src/node_buffer.h
  src/node_events.h
  src/node_version.h
  ${PROJECT_BINARY_DIR}/src/node_config.h

  DESTINATION include/node
)
