include(GNUInstallDirs)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/protobuf.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/protobuf.pc @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/protobuf-lite.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/protobuf-lite.pc @ONLY)

set(_protobuf_libraries libprotobuf-lite libprotobuf)
if (protobuf_BUILD_PROTOC_BINARIES)
    list(APPEND _protobuf_libraries libprotoc)
endif (protobuf_BUILD_PROTOC_BINARIES)

foreach(_library ${_protobuf_libraries})
  set_property(TARGET ${_library}
    PROPERTY INTERFACE_INCLUDE_DIRECTORIES
    $<BUILD_INTERFACE:${protobuf_source_dir}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  if (UNIX AND NOT APPLE)
    set_property(TARGET ${_library}
      PROPERTY INSTALL_RPATH "$ORIGIN")
  elseif (APPLE)
    set_property(TARGET ${_library}
      PROPERTY INSTALL_RPATH "@loader_path")
  endif()
  install(TARGETS ${_library} EXPORT protobuf-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${_library}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${_library}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${_library})
endforeach()

if (protobuf_BUILD_PROTOC_BINARIES)
  install(TARGETS protoc EXPORT protobuf-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT protoc)
  if (UNIX AND NOT APPLE)
    set_property(TARGET protoc
      PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  elseif (APPLE)
    set_property(TARGET protoc
      PROPERTY INSTALL_RPATH "@loader_path/../lib")
  endif()
endif (protobuf_BUILD_PROTOC_BINARIES)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/protobuf.pc ${CMAKE_CURRENT_BINARY_DIR}/protobuf-lite.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

file(STRINGS extract_includes.bat.in _extract_strings
  REGEX "^copy")
foreach(_extract_string ${_extract_strings})
  string(REGEX REPLACE "^.* .+ include\\\\(.+)$" "\\1"
    _header ${_extract_string})
  string(REPLACE "\\" "/" _header ${_header})
  get_filename_component(_extract_from "${protobuf_SOURCE_DIR}/../src/${_header}" ABSOLUTE)
  get_filename_component(_extract_name ${_header} NAME)
  get_filename_component(_extract_to "${CMAKE_INSTALL_INCLUDEDIR}/${_header}" PATH)
  if(EXISTS "${_extract_from}")
    install(FILES "${_extract_from}"
      DESTINATION "${_extract_to}"
      COMPONENT protobuf-headers
      RENAME "${_extract_name}")
  else()
    message(AUTHOR_WARNING "The file \"${_extract_from}\" is listed in "
      "\"${protobuf_SOURCE_DIR}/cmake/extract_includes.bat.in\" "
      "but there not exists. The file will not be installed.")
  endif()
endforeach()

# Internal function for parsing auto tools scripts
function(_protobuf_auto_list FILE_NAME VARIABLE)
  file(STRINGS ${FILE_NAME} _strings)
  set(_list)
  foreach(_string ${_strings})
    set(_found)
    string(REGEX MATCH "^[ \t]*${VARIABLE}[ \t]*=[ \t]*" _found "${_string}")
    if(_found)
      string(LENGTH "${_found}" _length)
      string(SUBSTRING "${_string}" ${_length} -1 _draft_list)
      foreach(_item ${_draft_list})
        string(STRIP "${_item}" _item)
        list(APPEND _list "${_item}")
      endforeach()
    endif()
  endforeach()
  set(${VARIABLE} ${_list} PARENT_SCOPE)
endfunction()

# Install well-known type proto files
_protobuf_auto_list("../src/Makefile.am" nobase_dist_proto_DATA)
foreach(_file ${nobase_dist_proto_DATA})
  get_filename_component(_file_from "../src/${_file}" ABSOLUTE)
  get_filename_component(_file_name ${_file} NAME)
  get_filename_component(_file_path ${_file} PATH)
  if(EXISTS "${_file_from}")
    install(FILES "${_file_from}"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${_file_path}"
      COMPONENT protobuf-protos
      RENAME "${_file_name}")
  else()
    message(AUTHOR_WARNING "The file \"${_file_from}\" is listed in "
      "\"${protobuf_SOURCE_DIR}/../src/Makefile.am\" as nobase_dist_proto_DATA "
      "but there not exists. The file will not be installed.")
  endif()
endforeach()

# Install configuration
set(_cmakedir_desc "Directory relative to CMAKE_INSTALL to install the cmake configuration files")
if(NOT MSVC)
  set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/protobuf" CACHE STRING "${_cmakedir_desc}")
else()
  set(CMAKE_INSTALL_CMAKEDIR "cmake" CACHE STRING "${_cmakedir_desc}")
endif()
mark_as_advanced(CMAKE_INSTALL_CMAKEDIR)

configure_file(protobuf-config.cmake.in
  ${CMAKE_INSTALL_CMAKEDIR}/protobuf-config.cmake @ONLY)
configure_file(protobuf-config-version.cmake.in
  ${CMAKE_INSTALL_CMAKEDIR}/protobuf-config-version.cmake @ONLY)
configure_file(protobuf-module.cmake.in
  ${CMAKE_INSTALL_CMAKEDIR}/protobuf-module.cmake @ONLY)
configure_file(protobuf-options.cmake
  ${CMAKE_INSTALL_CMAKEDIR}/protobuf-options.cmake @ONLY)

# Allows the build directory to be used as a find directory.

if (protobuf_BUILD_PROTOC_BINARIES)
  export(TARGETS libprotobuf-lite libprotobuf libprotoc protoc
    NAMESPACE protobuf::
    FILE ${CMAKE_INSTALL_CMAKEDIR}/protobuf-targets.cmake
  )
else (protobuf_BUILD_PROTOC_BINARIES)
  export(TARGETS libprotobuf-lite libprotobuf
    NAMESPACE protobuf::
    FILE ${CMAKE_INSTALL_CMAKEDIR}/protobuf-targets.cmake
  )
endif (protobuf_BUILD_PROTOC_BINARIES)

install(EXPORT protobuf-targets
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
  NAMESPACE protobuf::
  COMPONENT protobuf-export)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_INSTALL_CMAKEDIR}/
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
  COMPONENT protobuf-export
  PATTERN protobuf-targets.cmake EXCLUDE
)

option(protobuf_INSTALL_EXAMPLES "Install the examples folder" OFF)
if(protobuf_INSTALL_EXAMPLES)
  install(DIRECTORY ../examples/ DESTINATION examples
    COMPONENT protobuf-examples)
endif()
