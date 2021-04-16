#[=======================================================================[.rst:
FindLibevent2
-------------

Find the Libevent2 library. For now this finds the core library only.

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Libevent2::Libevent_core``
  The Libevent2 library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Libevent2_FOUND``
  If false, do not try to use Libevent2.
``LIBEVENT2_INCLUDE_DIR``
  where to find libevent headers.
``LIBEVENT2_LIBRARIES``
  the libraries needed to use Libevent2.
``LIBEVENT2_VERSION``
  the version of the Libevent2 library found

#]=======================================================================]

find_path(LIBEVENT2_INCLUDE_DIR event2/event.h
  HINTS
  "${LIBEVENT2_DIR}"
  "${LIBEVENT2_DIR}/include"
)

find_library(LIBEVENT2_LIBRARY NAMES event_core libevent_core
  HINTS
  "${LIBEVENT2_DIR}"
  "${LIBEVENT2_DIR}/lib"
)

set(LIBEVENT2_LIBRARIES "")

if (LIBEVENT2_INCLUDE_DIR AND LIBEVENT2_LIBRARY)
  if (NOT TARGET Libevent2::Libevent_core)
    add_library(Libevent2::Libevent_core UNKNOWN IMPORTED)
    set_target_properties(Libevent2::Libevent_core PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT2_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${LIBEVENT2_LIBRARY}"
      )
  endif ()

  if (NOT LIBEVENT2_VERSION AND LIBEVENT2_INCLUDE_DIR AND EXISTS "${LIBEVENT2_INCLUDE_DIR}/event2/event.h")
    file(STRINGS "${LIBEVENT2_INCLUDE_DIR}/event2/event-config.h" LIBEVENT2_H REGEX "^#define _?EVENT_+VERSION ")
    string(REGEX REPLACE "^.*EVENT_+VERSION \"([^\"]+)\".*$" "\\1" LIBEVENT2_VERSION "${LIBEVENT2_H}")
  endif ()
endif()

list(APPEND LIBEVENT2_LIBRARIES "${LIBEVENT2_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libevent2
  REQUIRED_VARS LIBEVENT2_LIBRARIES LIBEVENT2_INCLUDE_DIR
  VERSION_VAR LIBEVENT2_VERSION
  )

mark_as_advanced(LIBEVENT2_INCLUDE_DIR LIBEVENT2_LIBRARIES LIBEVENT2_LIBRARY)
