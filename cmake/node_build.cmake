#
# node build stuff
#

set(macros_file ${PROJECT_BINARY_DIR}/macros.py)

# replace debug(x) and assert(x) with nothing in release build
if(${CMAKE_BUILD_TYPE} MATCHES Release)
  file(APPEND ${macros_file} "macro debug(x) = void(0);\n")
  file(APPEND ${macros_file} "macro assert(x) = void(0);\n")
endif()

if(NOT DTRACE)
  set(dtrace_probes
    DTRACE_HTTP_CLIENT_REQUEST
    DTRACE_HTTP_CLIENT_RESPONSE
    DTRACE_HTTP_SERVER_REQUEST
    DTRACE_HTTP_SERVER_RESPONSE
    DTRACE_NET_SERVER_CONNECTION
    DTRACE_NET_STREAM_END
    DTRACE_NET_SOCKET_READ
    DTRACE_NET_SOCKET_WRITE)
  foreach(probe ${dtrace_probes})
    file(APPEND ${macros_file} "macro ${probe}(x) = void(0);\n")
  endforeach()
endif()

# Sort the JS files being built into natives so that the build is
# deterministic
list(SORT js2c_files)

# include macros file in generation
set(js2c_files ${js2c_files} ${macros_file})

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/src/node_natives.h
  COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_BINARY_DIR}/tools/js2c.py ${PROJECT_BINARY_DIR}/src/node_natives.h ${js2c_files}
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
  src/node_string.cc
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
  set(CPPFLAGS "${CPPFLAGS} -D${def}")
endforeach()

configure_file(src/node_config.h.in ${PROJECT_BINARY_DIR}/src/node_config.h ESCAPE_QUOTES)
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

if(DTRACE)
  add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src/node_provider.h
    COMMAND ${dtrace_bin} -x nolibs -h -o ${PROJECT_BINARY_DIR}/src/node_provider.h -s ${PROJECT_SOURCE_DIR}/src/node_provider.d
    DEPENDS ${PROJECT_SOURCE_DIR}/src/node_provider.d)

  set(node_sources ${node_sources} src/node_provider.o)
  set(node_sources src/node_provider.h ${node_sources})
endif()

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

if(DTRACE)
  # manually gather up the object files for dtrace
  get_property(sourcefiles TARGET node PROPERTY SOURCES)
  foreach(src_file ${sourcefiles})
    if(src_file MATCHES ".*\\.cc$")
      set(node_objs ${node_objs} ${PROJECT_BINARY_DIR}/CMakeFiles/node.dir/${src_file}.o)
    endif()
  endforeach()

  add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src/node_provider.o
    #COMMAND cmake -E echo ${node_objs}
    COMMAND ${dtrace_bin} -G -x nolibs -s ${PROJECT_SOURCE_DIR}/src/node_provider.d -o ${PROJECT_BINARY_DIR}/src/node_provider.o ${node_objs}
    DEPENDS ${node_objs})
endif()

install(TARGETS node RUNTIME DESTINATION bin)
install(FILES     
  ${PROJECT_BINARY_DIR}/config.h
  src/node.h
  src/node_object_wrap.h
  src/node_buffer.h
  src/node_events.h
  src/node_version.h
  ${PROJECT_BINARY_DIR}/src/node_config.h

  DESTINATION ${NODE_INCLUDE_PREFIX}/include/node
)
