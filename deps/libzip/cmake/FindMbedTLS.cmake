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
FindMbedTLS
-------

Finds the Mbed TLS library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``MbedTLS::MbedTLS``
  The Mbed TLS library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``MbedTLS_FOUND``
  True if the system has the Mbed TLS library.
``MbedTLS_VERSION``
  The version of the Mbed TLS library which was found.
``MbedTLS_INCLUDE_DIRS``
  Include directories needed to use Mbed TLS.
``MbedTLS_LIBRARIES``
  Libraries needed to link to Mbed TLS.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``MbedTLS_INCLUDE_DIR``
  The directory containing ``mbedtls/aes.h``.
``MbedTLS_LIBRARY``
  The path to the Mbed TLS library.

#]=======================================================================]

# I'm not aware of a pkg-config file for mbedtls as of 2020/07/08.
#find_package(PkgConfig)
#pkg_check_modules(PC_MbedTLS QUIET mbedtls)

find_path(MbedTLS_INCLUDE_DIR
  NAMES mbedtls/aes.h
#  PATHS ${PC_MbedTLS_INCLUDE_DIRS}
)
find_library(MbedTLS_LIBRARY
  NAMES mbedcrypto
#  PATHS ${PC_MbedTLS_LIBRARY_DIRS}
)

# Extract version information from the header file
if(MbedTLS_INCLUDE_DIR)
  # for major version 3
  if(EXISTS ${MbedTLS_INCLUDE_DIR}/mbedtls/build_info.h)
    file(STRINGS ${MbedTLS_INCLUDE_DIR}/mbedtls/build_info.h _ver_line
        REGEX "^#define MBEDTLS_VERSION_STRING  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
        LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
        MbedTLS_VERSION "${_ver_line}")
    unset(_ver_line)
  # for major version 2
  elseif(EXISTS ${MbedTLS_INCLUDE_DIR}/mbedtls/version.h)
    file(STRINGS ${MbedTLS_INCLUDE_DIR}/mbedtls/version.h _ver_line
         REGEX "^#define MBEDTLS_VERSION_STRING  *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           MbedTLS_VERSION "${_ver_line}")
    unset(_ver_line)
  else()
    if(PC_MbedTLS_VERSION)
      set(MbedTLS_VERSION ${PC_MbedTLS_VERSION})
    else()
      # version unknown
      set(MbedTLS_VERSION "0.0")
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MbedTLS
  FOUND_VAR MbedTLS_FOUND
  REQUIRED_VARS
    MbedTLS_LIBRARY
    MbedTLS_INCLUDE_DIR
  VERSION_VAR MbedTLS_VERSION
)

if(MbedTLS_FOUND)
  set(MbedTLS_LIBRARIES ${MbedTLS_LIBRARY})
  set(MbedTLS_INCLUDE_DIRS ${MbedTLS_INCLUDE_DIR})
#  set(MbedTLS_DEFINITIONS ${PC_MbedTLS_CFLAGS_OTHER})
endif()

if(MbedTLS_FOUND AND NOT TARGET MbedTLS::MbedTLS)
  add_library(MbedTLS::MbedTLS UNKNOWN IMPORTED)
  set_target_properties(MbedTLS::MbedTLS PROPERTIES
    IMPORTED_LOCATION "${MbedTLS_LIBRARY}"
#    INTERFACE_COMPILE_OPTIONS "${PC_MbedTLS_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${MbedTLS_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  MbedTLS_INCLUDE_DIR
  MbedTLS_LIBRARY
  )
