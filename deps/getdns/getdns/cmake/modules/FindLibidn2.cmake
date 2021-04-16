#[=======================================================================[.rst:
FindLibidn2
-----------

Find the Libidn2 library

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Libidn2::Libidn2``
  The Libidn2 library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Libidn2_FOUND``
  If false, do not try to use Libidn2.
``LIBIDN2_INCLUDE_DIR``
  where to find libidn2 headers.
``LIBIDN2_LIBRARIES``
  the libraries needed to use Libidn2.
``LIBIDN2_VERSION``
  the version of the Libidn2 library found

#]=======================================================================]

find_path(LIBIDN2_INCLUDE_DIR idn2.h
  HINTS
  "${LIBIDN2_DIR}"
  "${LIBIDN2_DIR}/include"
)

find_library(LIBIDN2_LIBRARY NAMES idn2 libidn2
  HINTS
  "${LIBIDN2_DIR}"
  "${LIBIDN2_DIR}/lib"
)

set(LIBIDN2_LIBRARIES "")

if (LIBIDN2_INCLUDE_DIR AND LIBIDN2_LIBRARY)
  if (NOT TARGET Libidn2::Libidn2)
    add_library(Libidn2::Libidn2 UNKNOWN IMPORTED)
    set_target_properties(Libidn2::Libidn2 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBIDN2_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${LIBIDN2_LIBRARY}"
      )
  endif ()

  if (NOT LIBIDN2_VERSION AND LIBIDN2_INCLUDE_DIR AND EXISTS "${LIBIDN2_INCLUDE_DIR}/unbound.h")
    file(STRINGS "${LIBIDN2_INCLUDE_DIR}/idn2.h" LIBIDN2_H REGEX "^#define IDN2_VERSION ")
    string(REGEX REPLACE "^.*IDN2_VERSION \"([0-9.]+)\".*$" "\\1" LIBIDN2_VERSION "${LIBIDN2_H}")
  endif ()
endif()

list(APPEND LIBIDN2_LIBRARIES "${LIBIDN2_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libidn2
  REQUIRED_VARS LIBIDN2_LIBRARIES LIBIDN2_INCLUDE_DIR
  VERSION_VAR LIBIDN2_VERSION
  )

mark_as_advanced(LIBIDN2_INCLUDE_DIR LIBIDN2_LIBRARIES LIBIDN2_LIBRARY)
