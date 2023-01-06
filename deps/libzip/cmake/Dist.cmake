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
Dist
-------

Provide ``dist`` and ``distcheck`` targets similar to
autoconf/automake functionality.

The ``dist`` target creates tarballs of the project in ``.tar.gz`` and
``.tar.xz`` formats.

The ``distcheck`` target extracts one of created tarballs, builds the
software using its defaults, and runs the tests.

Both targets use Unix shell commands.

The Dist target takes one argument, the file name (before the extension).

The ``distcheck`` target creates (and removes) ``${ARCHIVE_NAME}-build``
and ``${ARCHIVE_NAME}-dest``.

#]=======================================================================]
function(Dist ARCHIVE_NAME)
  if(NOT TARGET dist AND NOT TARGET distcheck)
    add_custom_target(dist
      COMMAND git config tar.tar.xz.command "xz -c"
      COMMAND git archive --prefix=${ARCHIVE_NAME}/ -o ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz HEAD
      COMMAND git archive --prefix=${ARCHIVE_NAME}/ -o ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.xz HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
    add_custom_target(distcheck
      COMMAND chmod -R u+w ${ARCHIVE_NAME} ${ARCHIVE_NAME}-build ${ARCHIVE_NAME}-dest 2>/dev/null || true
      COMMAND rm -rf ${ARCHIVE_NAME} ${ARCHIVE_NAME}-build ${ARCHIVE_NAME}-dest
      COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
      COMMAND chmod -R u-w ${ARCHIVE_NAME}
      COMMAND mkdir ${ARCHIVE_NAME}-build
      COMMAND mkdir ${ARCHIVE_NAME}-dest
      COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${ARCHIVE_NAME}-dest ${ARCHIVE_NAME} -B ${ARCHIVE_NAME}-build
      COMMAND make -C ${ARCHIVE_NAME}-build -j4
      COMMAND make -C ${ARCHIVE_NAME}-build test
      COMMAND make -C ${ARCHIVE_NAME}-build install
      #  COMMAND make -C ${ARCHIVE_NAME}-build uninstall
      #  COMMAND if [ `find ${ARCHIVE_NAME}-dest ! -type d | wc -l` -ne 0 ]; then echo leftover files in ${ARCHIVE_NAME}-dest; false; fi
      COMMAND make -C ${ARCHIVE_NAME}-build clean
      COMMAND chmod -R u+w ${ARCHIVE_NAME} ${ARCHIVE_NAME}-build ${ARCHIVE_NAME}-dest
      COMMAND rm -rf ${ARCHIVE_NAME} ${ARCHIVE_NAME}-build ${ARCHIVE_NAME}-dest
      COMMAND echo "${ARCHIVE_NAME}.tar.gz is ready for distribution."
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
    add_dependencies(distcheck dist)
  endif()
endfunction()
