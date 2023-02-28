# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindBZip2
---------

Try to find BZip2

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``BZip2::BZip2``, if
BZip2 has been found.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``BZIP2_FOUND``
  system has BZip2
``BZIP2_INCLUDE_DIRS``
  the BZip2 include directories
``BZIP2_LIBRARIES``
  Link these to use BZip2
``BZIP2_NEED_PREFIX``
  this is set if the functions are prefixed with ``BZ2_``
``BZIP2_VERSION_STRING``
  the version of BZip2 found

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``BZIP2_INCLUDE_DIR``
  the BZip2 include directory
#]=======================================================================]

set(_BZIP2_PATHS PATHS
  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Bzip2;InstallPath]"
  )

find_path(BZIP2_INCLUDE_DIR bzlib.h ${_BZIP2_PATHS} PATH_SUFFIXES include)

if (NOT BZIP2_LIBRARIES)
    find_library(BZIP2_LIBRARY_RELEASE NAMES bz2 bzip2 libbz2 libbzip2 ${_BZIP2_PATHS} PATH_SUFFIXES lib)
    find_library(BZIP2_LIBRARY_DEBUG NAMES bz2d bzip2d libbz2d libbzip2d ${_BZIP2_PATHS} PATH_SUFFIXES lib)

    include(${CMAKE_CURRENT_LIST_DIR}/SelectLibraryConfigurations.cmake)
    SELECT_LIBRARY_CONFIGURATIONS(BZIP2)
else ()
    file(TO_CMAKE_PATH "${BZIP2_LIBRARIES}" BZIP2_LIBRARIES)
endif ()

if (BZIP2_INCLUDE_DIR AND EXISTS "${BZIP2_INCLUDE_DIR}/bzlib.h")
    file(STRINGS "${BZIP2_INCLUDE_DIR}/bzlib.h" BZLIB_H REGEX "bzip2/libbzip2 version [0-9]+\\.[^ ]+ of [0-9]+ ")
    string(REGEX REPLACE ".* bzip2/libbzip2 version ([0-9]+\\.[^ ]+) of [0-9]+ .*" "\\1" BZIP2_VERSION_STRING "${BZLIB_H}")
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(BZip2
                                  REQUIRED_VARS BZIP2_LIBRARIES BZIP2_INCLUDE_DIR
                                  VERSION_VAR BZIP2_VERSION_STRING)

if (BZIP2_FOUND)
  set(BZIP2_INCLUDE_DIRS ${BZIP2_INCLUDE_DIR})
  include(${CMAKE_CURRENT_LIST_DIR}/CheckSymbolExists.cmake)
  include(${CMAKE_CURRENT_LIST_DIR}/CMakePushCheckState.cmake)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_QUIET ${BZip2_FIND_QUIETLY})
  set(CMAKE_REQUIRED_INCLUDES ${BZIP2_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${BZIP2_LIBRARIES})
  CHECK_SYMBOL_EXISTS(BZ2_bzCompressInit "bzlib.h" BZIP2_NEED_PREFIX)
  cmake_pop_check_state()

  if(NOT TARGET BZip2::BZip2)
    add_library(BZip2::BZip2 UNKNOWN IMPORTED)
    set_target_properties(BZip2::BZip2 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${BZIP2_INCLUDE_DIRS}")

    if(BZIP2_LIBRARY_RELEASE)
      set_property(TARGET BZip2::BZip2 APPEND PROPERTY
        IMPORTED_CONFIGURATIONS RELEASE)
      set_target_properties(BZip2::BZip2 PROPERTIES
        IMPORTED_LOCATION_RELEASE "${BZIP2_LIBRARY_RELEASE}")
    endif()

    if(BZIP2_LIBRARY_DEBUG)
      set_property(TARGET BZip2::BZip2 APPEND PROPERTY
        IMPORTED_CONFIGURATIONS DEBUG)
      set_target_properties(BZip2::BZip2 PROPERTIES
        IMPORTED_LOCATION_DEBUG "${BZIP2_LIBRARY_DEBUG}")
    endif()

    if(NOT BZIP2_LIBRARY_RELEASE AND NOT BZIP2_LIBRARY_DEBUG)
      set_property(TARGET BZip2::BZip2 APPEND PROPERTY
        IMPORTED_LOCATION "${BZIP2_LIBRARY}")
    endif()
  endif()
endif ()

mark_as_advanced(BZIP2_INCLUDE_DIR)
