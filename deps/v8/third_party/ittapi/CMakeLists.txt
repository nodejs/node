#/* <copyright>
#  This file is provided under a dual BSD/GPLv2 license.  When using or
#  redistributing this file, you may do so under either license.
#
#  GPL LICENSE SUMMARY
#
#  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution
#  in the file called LICENSE.GPL.
#
#  Contact Information:
#  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/
#
#  BSD LICENSE
#
#  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#    * Neither the name of Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#</copyright> */
#********************************************************************************************************************************************************************************************************************************************************************************************

cmake_minimum_required(VERSION 2.8)

project(ittapi)

OPTION(FORCE_32 "Force a 32bit compile on 64bit" OFF)

IF(FORCE_32 AND UNIX)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
ENDIF()

if(CMAKE_SIZEOF_VOID_P MATCHES "8" AND NOT(FORCE_32))
    set(ARCH_64 1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG)
    if (NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
    endif()
else()
    if (NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    endif()
    add_definitions(-DNDEBUG)
endif()

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG}     ${LIBRARY_OUTPUT_PATH} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(ITT_PUBLIC_HDRS
    include/ittnotify.h
    include/jitprofiling.h
    include/libittnotify.h
)

file(GLOB ITT_SRCS "src/ittnotify/*.c" "src/ittnotify/*.h")

add_library(ittnotify STATIC ${ITT_SRCS} ${ITT_PUBLIC_HDRS})

if(APPLE)
    SET_TARGET_PROPERTIES(ittnotify PROPERTIES OUTPUT_NAME ittnotify)
endif()

if(WIN32)
    if(ARCH_64)
        SET_TARGET_PROPERTIES(ittnotify PROPERTIES OUTPUT_NAME libittnotify64)
    else()
        SET_TARGET_PROPERTIES(ittnotify PROPERTIES OUTPUT_NAME libittnotify32)
    endif()
else()
    if(ARCH_64)
        SET_TARGET_PROPERTIES(ittnotify PROPERTIES OUTPUT_NAME ittnotify64)
    else()
        SET_TARGET_PROPERTIES(ittnotify PROPERTIES OUTPUT_NAME ittnotify32)
    endif()
endif()

if (NOT WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    TARGET_LINK_LIBRARIES(ittnotify dl)
endif()

SET_TARGET_PROPERTIES(ittnotify PROPERTIES LINKER_LANGUAGE C)

target_include_directories(ittnotify
    PUBLIC include src/ittnotify
)

set(CMAKE_SUPPRESS_REGENERATION true)
