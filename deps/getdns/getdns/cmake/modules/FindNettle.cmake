#[=======================================================================[.rst:
FindNettle
----------

Find the Nettle library.

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Nettle::Nettle``
  The Nettle library, if found.
``Nettle::Hogweed``
  The Hogweed library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Nettle_FOUND``
  If false, do not try to use Nettle.
``NETTLE_INCLUDE_DIR``
  where to find Nettle headers.
``NETTLE_LIBRARIES``
  the libraries needed to use Nettle.
``NETTLE_VERSION``
  the version of the Nettle library found

#]=======================================================================]

find_path(NETTLE_INCLUDE_DIR nettle/version.h
  HINTS
  "${NETTLE_DIR}"
  "${NETTLE_DIR}/include"
)

find_library(NETTLE_LIBRARY NAMES nettle libnettle
  HINTS
  "${NETTLE_DIR}"
  "${NETTLE_DIR}/lib"
)

find_library(HOGWEED_LIBRARY NAMES hogweed libhogweed
  HINTS
  "${NETTLE_DIR}"
  "${NETTLE_DIR}/lib"
)

set(NETTLE_LIBRARIES "")

# May need gmp library on Unix.
if (UNIX)
  find_library(NETTLE_GMP_LIBRARY gmp)

  if (NETTLE_GMP_LIBRARY)
    list(APPEND NETTLE_LIBRARIES "${NETTLE_GMP_LIBRARY}")
  endif ()
endif ()

if (NETTLE_INCLUDE_DIR AND NETTLE_LIBRARY AND HOGWEED_LIBRARY)
  if (NOT TARGET Nettle::Nettle)
    add_library(Nettle::Nettle UNKNOWN IMPORTED)
    set_target_properties(Nettle::Nettle PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NETTLE_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${NETTLE_LIBRARIES}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${NETTLE_LIBRARY}"
      )
  endif ()
  if (NOT TARGET Nettle::Hogweed)
    add_library(Nettle::Hogweed UNKNOWN IMPORTED)
    set_target_properties(Nettle::Hogweed PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NETTLE_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${HOGWEED_LIBRARY}"
      )
  endif ()

  if (NOT NETTLE_VERSION AND NETTLE_INCLUDE_DIR)
    file(STRINGS "${NETTLE_INCLUDE_DIR}/nettle/version.h" NETTLE_VER_H REGEX "^#define NETTLE_VERSION_(MAJOR|MINOR) ")
    string(REGEX REPLACE "^.*_MAJOR ([0-9]+).*_MINOR ([0-9]+).*$" "\\1.\\2" NETTLE_VERSION "${NETTLE_VER_H}")
  endif ()
endif()

list(APPEND NETTLE_LIBRARIES "${NETTLE_LIBRARY}" "${HOGWEED_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nettle
  REQUIRED_VARS NETTLE_LIBRARIES NETTLE_INCLUDE_DIR
  VERSION_VAR NETTLE_VERSION
  )

mark_as_advanced(NETTLE_INCLUDE_DIR NETTLE_LIBRARIES NETTLE_LIBRARY HOGWEED_LIBRARY)
