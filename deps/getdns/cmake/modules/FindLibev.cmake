#[=======================================================================[.rst:
FindLibev
---------

Find the Libev library.

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Libev::Libev``
  The Libev library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Libev_FOUND``
  If false, do not try to use Libev.
``LIBEV_INCLUDE_DIR``
  where to find libev headers.
``LIBEV_LIBRARIES``
  the libraries needed to use Libev.
``LIBEV_VERSION``
  the version of the Libev library found

#]=======================================================================]

find_path(LIBEV_INCLUDE_DIR ev.h
  HINTS
  "${LIBEV_DIR}"
  "${LIBEV_DIR}/include"
)

find_library(LIBEV_LIBRARY NAMES ev libev
  HINTS
  "${LIBEV_DIR}"
  "${LIBEV_DIR}/lib"
)

set(LIBEV_LIBRARIES "")

if (LIBEV_INCLUDE_DIR AND LIBEV_LIBRARY)
  if (NOT TARGET Libev::Libev)
    add_library(Libev::Libev UNKNOWN IMPORTED)
    set_target_properties(Libev::Libev PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBEV_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${LIBEV_LIBRARY}"
      )
  endif ()
endif()

list(APPEND LIBEV_LIBRARIES "${LIBEV_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libev
  REQUIRED_VARS LIBEV_LIBRARIES LIBEV_INCLUDE_DIR
  )

mark_as_advanced(LIBEV_INCLUDE_DIR LIBEV_LIBRARIES LIBEV_LIBRARY)
