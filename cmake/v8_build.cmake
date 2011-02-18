#
# v8 build stuff
#

string(TOLOWER ${CMAKE_BUILD_TYPE} v8mode)
set(v8arch ${node_arch})

if(${node_arch} MATCHES x86_64)
  set(v8arch x64)
elseif(${node_arch} MATCHES x86)
  set(v8arch ia32)
endif()


if(NOT SHARED_V8)
  if(V8_SNAPSHOT)
    set(v8_snapshot snapshot=on)
  endif()

  if(V8_OPROFILE)
    set(v8_oprofile prof=oprofile)
  endif()

  if(V8_GDBJIT)
    set(v8_gdbjit gdbjit=on)
  endif()
  
  if(${node_platform} MATCHES darwin)
    execute_process(COMMAND hwprefs cpu_count OUTPUT_VARIABLE cpu_count)
  elseif(${node_platform} MATCHES linux)
    execute_process(COMMAND sh -c "cat /proc/cpuinfo | grep processor | sort | uniq | wc -l"
      OUTPUT_VARIABLE cpu_count)
  elseif(${node_platform} MATCHES sunos)
    execute_process(COMMAND sh -c "psrinfo | wc -l" OUTPUT_VARIABLE cpu_count)
  else()
    set(cpu_count 1)
  endif()

  if(${cpu_count} GREATER 1)
    math(EXPR parallel_jobs ${cpu_count}*2)
  else()
    set(parallel_jobs 1)
  endif()

  add_library(v8 STATIC IMPORTED)
  set_property(TARGET v8
    PROPERTY IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/deps/v8/${v8_fn})

  set(compile_env_vars  "CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB} CFLAGS=\"${CMAKE_C_FLAGS}\" CXXFLAGS=\"${CMAKE_CXX_FLAGS}\" LDFLAGS=\"${CMAKE_EXE_LINKER_FLAGS}\"")

  set(compile_cmd "${compile_env_vars} ${PYTHON_EXECUTABLE} ${PROJECT_BINARY_DIR}/tools/scons/scons.py -j ${parallel_jobs} visibility=default mode=${v8mode} arch=${v8arch} library=static ${v8_snapshot} ${v8_oprofile} ${v8_gdbjit} verbose=on")


  if(CMAKE_VERSION VERSION_GREATER 2.8 OR CMAKE_VERSION VERSION_EQUAL 2.8)
    # use ExternalProject for CMake >2.8
    include(ExternalProject)
    
    ExternalProject_Add(v8_extprj
      URL ${PROJECT_SOURCE_DIR}/deps/v8
      
      BUILD_IN_SOURCE True
      BUILD_COMMAND sh -c "${compile_cmd}"
      SOURCE_DIR ${PROJECT_BINARY_DIR}/deps/v8
      # ignore this stuff, it's not needed for building v8 but ExternalProject
      # demands these steps
      
      CONFIGURE_COMMAND "true" # fake configure
      INSTALL_COMMAND "true" # fake install
      )
    
    add_dependencies(node v8_extprj)
  else()
    # copy v8 sources inefficiently with CMake versions <2.8
    file(GLOB_RECURSE v8_sources RELATIVE ${PROJECT_SOURCE_DIR} deps/v8/*)
    
    if(NOT ${in_source_build})
      file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/deps/v8)
      
      foreach(FILE ${v8_sources})
        add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/${FILE}
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/${FILE} ${PROJECT_BINARY_DIR}/${FILE}
          DEPENDS ${PROJECT_SOURCE_DIR}/${FILE}
          )
        list(APPEND v8_sources_dest ${PROJECT_BINARY_DIR}/${FILE})
      endforeach()
    else()
      set(v8_sources_dest ${v8_sources})
    endif()

    add_custom_command(
      OUTPUT ${PROJECT_BINARY_DIR}/deps/v8/${v8_fn}
      COMMAND sh -c "${compile_cmd}"
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/deps/v8/
      DEPENDS ${v8_sources_dest}
    )

    add_custom_target(v8_stock ALL DEPENDS ${PROJECT_BINARY_DIR}/deps/v8/${v8_fn})
    set_property(TARGET v8 PROPERTY
      IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/deps/v8/${v8_fn})
    add_dependencies(node v8_stock)
  endif()
endif()
