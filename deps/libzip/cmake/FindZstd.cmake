# Copyright (C) 2020 Dieter Baron and Thomas Klausner
#
# The authors can be contacted at <info@libzip.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#
# 3. The names of the authors may not be used to endorse or promote
#   products derived from this software without specific prior
#   written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindZstd
-------

Finds the Zstandard (zstd) library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Zstd::Zstd``
  The Zstandard library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Zstd_FOUND``
  True if the system has the Zstandard library.
``Zstd_VERSION``
  The version of the Zstandard library which was found.
``Zstd_INCLUDE_DIRS``
  Include directories needed to use Zstandard.
``Zstd_LIBRARIES``
  Libraries needed to link to Zstandard.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Zstd_INCLUDE_DIR``
  The directory containing ``zstd.h``.
``Zstd_LIBRARY``
  The path to the Zstandard library.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_Zstd QUIET zstd)

find_path(Zstd_INCLUDE_DIR
  NAMES zstd.h
  PATHS ${PC_Zstd_INCLUDE_DIRS}
)
find_library(Zstd_LIBRARY
  NAMES zstd
  PATHS ${PC_Zstd_LIBRARY_DIRS}
)

# Extract version information from the header file
if(Zstd_INCLUDE_DIR)
  file(STRINGS ${Zstd_INCLUDE_DIR}/zstd.h _ver_major_line
    REGEX "^#define ZSTD_VERSION_MAJOR  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    Zstd_MAJOR_VERSION "${_ver_major_line}")
  file(STRINGS ${Zstd_INCLUDE_DIR}/zstd.h _ver_minor_line
    REGEX "^#define ZSTD_VERSION_MINOR  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    Zstd_MINOR_VERSION "${_ver_minor_line}")
  file(STRINGS ${Zstd_INCLUDE_DIR}/zstd.h _ver_release_line
    REGEX "^#define ZSTD_VERSION_RELEASE  *[0-9]+"
    LIMIT_COUNT 1)
  string(REGEX MATCH "[0-9]+"
    Zstd_RELEASE_VERSION "${_ver_release_line}")
  set(Zstd_VERSION "${Zstd_MAJOR_VERSION}.${Zstd_MINOR_VERSION}.${Zstd_RELEASE_VERSION}")
  unset(_ver_major_line)
  unset(_ver_minor_line)
  unset(_ver_release_line)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zstd
  FOUND_VAR Zstd_FOUND
  REQUIRED_VARS
    Zstd_LIBRARY
    Zstd_INCLUDE_DIR
  VERSION_VAR Zstd_VERSION
)

if(Zstd_FOUND)
  set(Zstd_LIBRARIES ${Zstd_LIBRARY})
  set(Zstd_INCLUDE_DIRS ${Zstd_INCLUDE_DIR})
  set(Zstd_DEFINITIONS ${PC_Zstd_CFLAGS_OTHER})
endif()

if(Zstd_FOUND AND NOT TARGET Zstd::Zstd)
  add_library(Zstd::Zstd UNKNOWN IMPORTED)
  set_target_properties(Zstd::Zstd PROPERTIES
    IMPORTED_LOCATION "${Zstd_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_Zstd_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Zstd_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  Zstd_INCLUDE_DIR
  Zstd_LIBRARY
)
