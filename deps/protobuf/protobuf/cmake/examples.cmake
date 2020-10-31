if(protobuf_VERBOSE)
  message(STATUS "Protocol Buffers Examples Configuring...")
endif()

get_filename_component(examples_dir "../examples" ABSOLUTE)

if(protobuf_VERBOSE)
  message(STATUS "Protocol Buffers Examples Configuring done")
endif()
include(ExternalProject)

# Internal utility function: Create a custom target representing a build of examples with custom options.
function(add_examples_build NAME)

  ExternalProject_Add(${NAME}
    PREFIX ${NAME}
    SOURCE_DIR "${examples_dir}"
    BINARY_DIR ${NAME}
    STAMP_DIR ${NAME}/logs
    INSTALL_COMMAND "" #Skip
    LOG_CONFIGURE 1
    CMAKE_CACHE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
                     "-Dprotobuf_VERBOSE:BOOL=${protobuf_VERBOSE}"
                     ${ARGN}
  )
  set_property(TARGET ${NAME} PROPERTY FOLDER "Examples")
  set_property(TARGET ${NAME} PROPERTY EXCLUDE_FROM_ALL TRUE)
endfunction()

# Add examples as an external project.
# sub_directory cannot be used because the find_package(protobuf) call would cause failures with redefined targets.
add_examples_build(examples "-Dprotobuf_DIR:PATH=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_CMAKEDIR}")
add_dependencies(examples libprotobuf protoc)

option(protobuf_BUILD_EXAMPLES_MULTITEST "Build Examples in multiple configurations. Useful for testing." OFF)
mark_as_advanced(protobuf_BUILD_EXAMPLES_MULTITEST)
if(protobuf_BUILD_EXAMPLES_MULTITEST)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  #Build using the legacy compatibility module.
  add_examples_build(examples-legacy
    "-Dprotobuf_DIR:PATH=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_CMAKEDIR}"
    "-Dprotobuf_MODULE_COMPATIBLE:BOOL=TRUE"
  )
  add_dependencies(examples-legacy libprotobuf protoc)

  #Build using the installed library.
  add_examples_build(examples-installed
    "-Dprotobuf_DIR:PATH=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_CMAKEDIR}"
  )

  #Build using the installed library in legacy compatibility mode.
  add_examples_build(examples-installed-legacy
    "-Dprotobuf_DIR:PATH=${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_CMAKEDIR}"
    "-Dprotobuf_MODULE_COMPATIBLE:BOOL=TRUE"
  )
endif()
