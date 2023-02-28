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
FindNettle
-------

Finds the Nettle library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Nettle::Nettle``
  The Nettle library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Nettle_FOUND``
  True if the system has the Nettle library.
``Nettle_VERSION``
  The version of the Nettle library which was found.
``Nettle_INCLUDE_DIRS``
  Include directories needed to use Nettle.
``Nettle_LIBRARIES``
  Libraries needed to link to Nettle.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Nettle_INCLUDE_DIR``
  The directory containing ``nettle/aes.h``.
``Nettle_LIBRARY``
  The path to the Nettle library.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_Nettle QUIET nettle)

find_path(Nettle_INCLUDE_DIR
  NAMES nettle/aes.h nettle/md5.h nettle/pbkdf2.h nettle/ripemd160.h nettle/sha.h
  PATHS ${PC_Nettle_INCLUDE_DIRS}
)
find_library(Nettle_LIBRARY
  NAMES nettle
  PATHS ${PC_Nettle_LIBRARY_DIRS}
)

# Extract version information from the header file
if(Nettle_INCLUDE_DIR)
  # This file only exists in nettle>=3.0
  if(EXISTS ${Nettle_INCLUDE_DIR}/nettle/version.h)
    file(STRINGS ${Nettle_INCLUDE_DIR}/nettle/version.h _ver_major_line
         REGEX "^#define NETTLE_VERSION_MAJOR  *[0-9]+"
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+"
           Nettle_MAJOR_VERSION "${_ver_major_line}")
    file(STRINGS ${Nettle_INCLUDE_DIR}/nettle/version.h _ver_minor_line
         REGEX "^#define NETTLE_VERSION_MINOR  *[0-9]+"
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+"
           Nettle_MINOR_VERSION "${_ver_minor_line}")
    set(Nettle_VERSION "${Nettle_MAJOR_VERSION}.${Nettle_MINOR_VERSION}")
    unset(_ver_major_line)
    unset(_ver_minor_line)
  else()
    if(PC_Nettle_VERSION)
      set(Nettle_VERSION ${PC_Nettle_VERSION})
    else()
      set(Nettle_VERSION "1.0")
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nettle
  FOUND_VAR Nettle_FOUND
  REQUIRED_VARS
    Nettle_LIBRARY
    Nettle_INCLUDE_DIR
  VERSION_VAR Nettle_VERSION
)

if(Nettle_FOUND)
  set(Nettle_LIBRARIES ${Nettle_LIBRARY})
  set(Nettle_INCLUDE_DIRS ${Nettle_INCLUDE_DIR})
  set(Nettle_DEFINITIONS ${PC_Nettle_CFLAGS_OTHER})
endif()

if(Nettle_FOUND AND NOT TARGET Nettle::Nettle)
  add_library(Nettle::Nettle UNKNOWN IMPORTED)
  set_target_properties(Nettle::Nettle PROPERTIES
    IMPORTED_LOCATION "${Nettle_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_Nettle_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Nettle_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  Nettle_INCLUDE_DIR
  Nettle_LIBRARY
)

# compatibility variables
set(Nettle_VERSION_STRING ${Nettle_VERSION})
