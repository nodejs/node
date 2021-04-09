#[=======================================================================[.rst:
FindLibuv
---------

Find the Libuv library.

Imported targets
^^^^^^^^^^^^^^^^

This module defines the following :prop_tgt:`IMPORTED` targets:

``Libuv::Libuv``
  The Libuv library, if found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``Libuv_FOUND``
  If false, do not try to use Libuv.
``LIBUV_INCLUDE_DIR``
  where to find libuv headers.
``LIBUV_LIBRARIES``
  the libraries needed to use Libuv.
``LIBUV_VERSION``
  the version of the Libuv library found

#]=======================================================================]

find_path(LIBUV_INCLUDE_DIR uv.h
  HINTS
  "${LIBUV_DIR}"
  "${LIBUV_DIR}/include"
)

find_library(LIBUV_LIBRARY NAMES uv libuv
  HINTS
  "${LIBUV_DIR}"
  "${LIBUV_DIR}/lib"
)

set(LIBUV_LIBRARIES "")

if (LIBUV_INCLUDE_DIR AND LIBUV_LIBRARY)
  if (NOT TARGET Libuv::Libuv)
    add_library(Libuv::Libuv UNKNOWN IMPORTED)
    set_target_properties(Libuv::Libuv PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LIBUV_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "C"
      IMPORTED_LOCATION "${LIBUV_LIBRARY}"
      )
  endif ()

  if (NOT LIBUV_VERSION AND LIBUV_INCLUDE_DIR)
    if (EXISTS "${LIBUV_INCLUDE_DIR}/uv-version.h")
      file(STRINGS "${LIBUV_INCLUDE_DIR}/uv-version.h" LIBUV_VER_H REGEX "^#define UV_VERSION_(MAJOR|MINOR|PATCH) ")
    elseif (EXISTS "${LIBUV_INCLUDE_DIR}/uv/version.h")
      file(STRINGS "${LIBUV_INCLUDE_DIR}/uv/version.h" LIBUV_VER_H REGEX "^#define UV_VERSION_(MAJOR|MINOR|PATCH) ")
    endif ()
    string(REGEX REPLACE "^.*_MAJOR ([0-9]+).*_MINOR ([0-9]+).*_PATCH ([0-9]+).*$" "\\1.\\2.\\3" LIBUV_VERSION "${LIBUV_VER_H}")
  endif ()
endif()

list(APPEND LIBUV_LIBRARIES "${LIBUV_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libuv
  REQUIRED_VARS LIBUV_LIBRARIES LIBUV_INCLUDE_DIR
  VERSION_VAR LIBUV_VERSION
  )

mark_as_advanced(LIBUV_INCLUDE_DIR LIBUV_LIBRARIES LIBUV_LIBRARY)
