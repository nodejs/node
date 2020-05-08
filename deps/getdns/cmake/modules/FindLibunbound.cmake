#[=======================================================================[.rst:
FindLibunbound
--------------

Find the Libunbound library

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Libunbound::Libunbound``
  The Libunbound library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Libunbound_FOUND``
  If false, do not try to use Libunbound.
``LIBUNBOUND_INCLUDE_DIR``
  where to find libunbound headers.
``LIBUNBOUND_LIBRARIES``
  the libraries needed to use Libunbound.
``LIBUNBOUND_VERSION``
  the version of the Libunbound library found

#]=======================================================================]

find_path(LIBUNBOUND_INCLUDE_DIR unbound.h
  HINTS
  "${LIBUNBOUND_DIR}"
  "${LIBUNBOUND_DIR}/include"
)

find_library(LIBUNBOUND_LIBRARY NAMES unbound
  HINTS
  "${LIBUNBOUND_DIR}"
  "${LIBUNBOUND_DIR}/lib"
)

set(LIBUNBOUND_LIBRARIES "")

if (UNIX)
  find_package(Threads REQUIRED)
  find_package(OpenSSL REQUIRED)

  list(APPEND LIBUNBOUND_LIBRARIES "${CMAKE_THREAD_LIBS_INIT}")
  list(APPEND LIBUNBOUND_LIBRARIES "${OPENSSL_LIBRARIES}")
endif()

if (LIBUNBOUND_INCLUDE_DIR AND LIBUNBOUND_LIBRARY)
  if (NOT TARGET Libunbound::Libunbound)
    add_library(Libunbound::Libunbound UNKNOWN IMPORTED)
    set_target_properties(Libunbound::Libunbound PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBUNBOUND_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${LIBUNBOUND_LIBRARY}"
      )

    if(UNIX AND TARGET Threads::Threads)
      set_property(TARGET Libunbound::Libunbound APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES Threads::Threads)
    endif ()
    if(UNIX AND TARGET OpenSSL::SSL)
      set_property(TARGET Libunbound::Libunbound APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES OpenSSL::SSL)
    endif ()
    if(UNIX AND TARGET OpenSSL::Crypto)
      set_property(TARGET Libunbound::Libunbound APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
    endif ()
  endif ()

  if (NOT LIBUNBOUND_VERSION AND LIBUNBOUND_INCLUDE_DIR AND EXISTS "${LIBUNBOUND_INCLUDE_DIR}/unbound.h")
    file(STRINGS "${LIBUNBOUND_INCLUDE_DIR}/unbound.h" LIBUNBOUND_H REGEX "^#define UNBOUND_VERSION_M[A-Z]+")
    string(REGEX REPLACE "^.*MAJOR ([0-9]+).*MINOR ([0-9]+).*MICRO ([0-9]+).*$" "\\1.\\2.\\3" LIBUNBOUND_VERSION "${LIBUNBOUND_H}")
  endif ()
endif()

list(APPEND LIBUNBOUND_LIBRARIES "${LIBUNBOUND_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libunbound
  REQUIRED_VARS LIBUNBOUND_LIBRARIES LIBUNBOUND_INCLUDE_DIR
  VERSION_VAR LIBUNBOUND_VERSION
  )

mark_as_advanced(LIBUNBOUND_INCLUDE_DIR LIBUNBOUND_LIBRARIES LIBUNBOUND_LIBRARY)
