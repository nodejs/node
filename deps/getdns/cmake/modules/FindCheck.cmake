#[=======================================================================[.rst:
FindCheck
--------

Find the Check (Unit Testing Framework for C) library

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Check::Check``
  The Check library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Check_FOUND``
  If false, do not try to use Check.
``CHECK_INCLUDE_DIR``
  where to find check.h, etc.
``CHECK_LIBRARIES``
  the libraries needed to use Check.
``CHECK_VERSION``
  the version of the Check library found

#]=======================================================================]

find_path(CHECK_INCLUDE_DIR check.h
  HINTS
  "${CHECK_DIR}"
  "${CHECK_DIR}/include"
  )

# Check for PIC and non-PIC libraries. If PIC present, use that
# in preference (as per Debian check.pc).
find_library(CHECK_LIBRARY NAMES check_pic libcheck_pic
  HINTS
  "${CHECK_DIR}"
  "${CHECK_DIR}/lib"
  )

if (NOT CHECK_LIBRARY)
  find_library(CHECK_LIBRARY NAMES check libcheck
    HINTS
    "${CHECK_DIR}"
    "${CHECK_DIR}/lib"
    )
endif ()

set(CHECK_LIBRARIES "")

# Check may need the math, subunit and rt libraries on Unix
if (UNIX)
  find_library(CHECK_MATH_LIBRARY m)
  find_library(CHECK_RT_LIBRARY rt)
  find_library(CHECK_SUBUNIT_LIBRARY subunit)

  if (CHECK_MATH_LIBRARY)
    list(APPEND CHECK_LIBRARIES "${CHECK_MATH_LIBRARY}")
  endif ()
  if (CHECK_RT_LIBRARY)
    list(APPEND CHECK_LIBRARIES "${CHECK_RT_LIBRARY}")
  endif ()
  if (CHECK_SUBUNIT_LIBRARY)
    list(APPEND CHECK_LIBRARIES "${CHECK_SUBUNIT_LIBRARY}")
  endif ()
endif()

if (CHECK_INCLUDE_DIR AND CHECK_LIBRARY)
  if (NOT TARGET Check::Check)
    add_library(Check::Check UNKNOWN IMPORTED)
    set_target_properties(Check::Check PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${CHECK_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${CHECK_LIBRARIES}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${CHECK_LIBRARY}"
      )
  endif ()

  if (NOT CHECK_VERSION AND CHECK_INCLUDE_DIR AND EXISTS "${CHECK_INCLUDE_DIR}/check.h")
    file(STRINGS "${CHECK_INCLUDE_DIR}/check.h" CHECK_H REGEX "^#define CHECK_M[A-Z]+_VERSION")
    string(REGEX REPLACE "^.*\(([0-9]+)\).*\(([0-9]+)\).*\(([0-9]+)\).*$" "\\1.\\2.\\3" CHECK_VERSION "${CHECK_H}")
  endif ()
endif()

list(APPEND CHECK_LIBRARIES "${CHECK_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Check
  REQUIRED_VARS CHECK_LIBRARIES CHECK_INCLUDE_DIR
  VERSION_VAR CHECK_VERSION
  )

mark_as_advanced(CHECK_INCLUDE_DIR CHECK_LIBRARIES CHECK_LIBRARY
  CHECK_MATH_LIBRARY CHECK_RT_LIBRARY CHECK_SUBUNIT_LIBRARY)
