# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLibLZMA
-----------

Find LZMA compression algorithm headers and library.


Imported Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``LibLZMA::LibLZMA``, if
liblzma has been found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``LIBLZMA_FOUND``
  True if liblzma headers and library were found.
``LIBLZMA_INCLUDE_DIRS``
  Directory where liblzma headers are located.
``LIBLZMA_LIBRARIES``
  Lzma libraries to link against.
``LIBLZMA_HAS_AUTO_DECODER``
  True if lzma_auto_decoder() is found (required).
``LIBLZMA_HAS_EASY_ENCODER``
  True if lzma_easy_encoder() is found (required).
``LIBLZMA_HAS_LZMA_PRESET``
  True if lzma_lzma_preset() is found (required).
``LIBLZMA_VERSION_MAJOR``
  The major version of lzma
``LIBLZMA_VERSION_MINOR``
  The minor version of lzma
``LIBLZMA_VERSION_PATCH``
  The patch version of lzma
``LIBLZMA_VERSION_STRING``
  version number as a string (ex: "5.0.3")
#]=======================================================================]

find_path(LIBLZMA_INCLUDE_DIR lzma.h )
if(NOT LIBLZMA_LIBRARY)
  find_library(LIBLZMA_LIBRARY_RELEASE NAMES lzma liblzma PATH_SUFFIXES lib)
  find_library(LIBLZMA_LIBRARY_DEBUG NAMES lzmad liblzmad PATH_SUFFIXES lib)
  include(${CMAKE_CURRENT_LIST_DIR}/SelectLibraryConfigurations.cmake)
  select_library_configurations(LIBLZMA)
else()
  file(TO_CMAKE_PATH "${LIBLZMA_LIBRARY}" LIBLZMA_LIBRARY)
endif()

if(LIBLZMA_INCLUDE_DIR AND EXISTS "${LIBLZMA_INCLUDE_DIR}/lzma/version.h")
    file(STRINGS "${LIBLZMA_INCLUDE_DIR}/lzma/version.h" LIBLZMA_HEADER_CONTENTS REGEX "#define LZMA_VERSION_[A-Z]+ [0-9]+")

    string(REGEX REPLACE ".*#define LZMA_VERSION_MAJOR ([0-9]+).*" "\\1" LIBLZMA_VERSION_MAJOR "${LIBLZMA_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define LZMA_VERSION_MINOR ([0-9]+).*" "\\1" LIBLZMA_VERSION_MINOR "${LIBLZMA_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define LZMA_VERSION_PATCH ([0-9]+).*" "\\1" LIBLZMA_VERSION_PATCH "${LIBLZMA_HEADER_CONTENTS}")

    set(LIBLZMA_VERSION_STRING "${LIBLZMA_VERSION_MAJOR}.${LIBLZMA_VERSION_MINOR}.${LIBLZMA_VERSION_PATCH}")
    unset(LIBLZMA_HEADER_CONTENTS)
endif()

# We're using new code known now as XZ, even library still been called LZMA
# it can be found in http://tukaani.org/xz/
# Avoid using old codebase
if (LIBLZMA_LIBRARY)
  include(${CMAKE_CURRENT_LIST_DIR}/CheckLibraryExists.cmake)
  set(CMAKE_REQUIRED_QUIET_SAVE ${CMAKE_REQUIRED_QUIET})
  set(CMAKE_REQUIRED_QUIET ${LibLZMA_FIND_QUIETLY})
  if(NOT LIBLZMA_LIBRARY_RELEASE AND NOT LIBLZMA_LIBRARY_DEBUG)
    set(LIBLZMA_LIBRARY_check ${LIBLZMA_LIBRARY})
  elseif(LIBLZMA_LIBRARY_RELEASE)
    set(LIBLZMA_LIBRARY_check ${LIBLZMA_LIBRARY_RELEASE})
  elseif(LIBLZMA_LIBRARY_DEBUG)
    set(LIBLZMA_LIBRARY_check ${LIBLZMA_LIBRARY_DEBUG})
  endif()
  CHECK_LIBRARY_EXISTS(${LIBLZMA_LIBRARY_check} lzma_auto_decoder "" LIBLZMA_HAS_AUTO_DECODER)
  CHECK_LIBRARY_EXISTS(${LIBLZMA_LIBRARY_check} lzma_easy_encoder "" LIBLZMA_HAS_EASY_ENCODER)
  CHECK_LIBRARY_EXISTS(${LIBLZMA_LIBRARY_check} lzma_lzma_preset "" LIBLZMA_HAS_LZMA_PRESET)
  unset(LIBLZMA_LIBRARY_check)
  set(CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET_SAVE})
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(LibLZMA  REQUIRED_VARS  LIBLZMA_LIBRARY
                                                          LIBLZMA_INCLUDE_DIR
                                                          LIBLZMA_HAS_AUTO_DECODER
                                                          LIBLZMA_HAS_EASY_ENCODER
                                                          LIBLZMA_HAS_LZMA_PRESET
                                           VERSION_VAR    LIBLZMA_VERSION_STRING
                                 )
mark_as_advanced( LIBLZMA_INCLUDE_DIR LIBLZMA_LIBRARY )

if (LIBLZMA_FOUND)
    set(LIBLZMA_LIBRARIES ${LIBLZMA_LIBRARY})
    set(LIBLZMA_INCLUDE_DIRS ${LIBLZMA_INCLUDE_DIR})
    if(NOT TARGET LibLZMA::LibLZMA)
        add_library(LibLZMA::LibLZMA UNKNOWN IMPORTED)
        set_target_properties(LibLZMA::LibLZMA PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES ${LIBLZMA_INCLUDE_DIR}
                              IMPORTED_LINK_INTERFACE_LANGUAGES C)

        if(LIBLZMA_LIBRARY_RELEASE)
            set_property(TARGET LibLZMA::LibLZMA APPEND PROPERTY
                IMPORTED_CONFIGURATIONS RELEASE)
            set_target_properties(LibLZMA::LibLZMA PROPERTIES
                IMPORTED_LOCATION_RELEASE "${LIBLZMA_LIBRARY_RELEASE}")
        endif()

        if(LIBLZMA_LIBRARY_DEBUG)
            set_property(TARGET LibLZMA::LibLZMA APPEND PROPERTY
                IMPORTED_CONFIGURATIONS DEBUG)
            set_target_properties(LibLZMA::LibLZMA PROPERTIES
                IMPORTED_LOCATION_DEBUG "${LIBLZMA_LIBRARY_DEBUG}")
        endif()

        if(NOT LIBLZMA_LIBRARY_RELEASE AND NOT LIBLZMA_LIBRARY_DEBUG)
            set_target_properties(LibLZMA::LibLZMA PROPERTIES
                IMPORTED_LOCATION "${LIBLZMA_LIBRARY}")
        endif()
    endif()
endif ()
