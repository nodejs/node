#!/usr/bin/env bash

# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will package a built gcmole plugin together with the
# corresponding clang binary into an archive which can be used on the
# buildbot infrastructure to be run against V8 checkouts.

THIS_DIR="$(readlink -f "$(dirname "${0}")")"

PACKAGE_DIR="${THIS_DIR}/gcmole-tools"
PACKAGE_FILE="${THIS_DIR}/gcmole-tools.tar.gz"
PACKAGE_SUM="${THIS_DIR}/gcmole-tools.tar.gz.sha1"
BUILD_DIR="${THIS_DIR}/bootstrap/build"

# Echo all commands
set -x

# Clean out any old files
if [ -e "${PACKAGE_DIR:?}/bin" ] ; then
  rm -rf "${PACKAGE_DIR:?}/bin"
fi
if [ -e "${PACKAGE_DIR:?}/lib" ] ; then
  rm -rf "${PACKAGE_DIR:?}/lib"
fi

# Copy all required files
mkdir -p "${PACKAGE_DIR}/bin"
cp "${BUILD_DIR}/bin/clang++" "${PACKAGE_DIR}/bin"
mkdir -p "${PACKAGE_DIR}/lib"
cp -r "${BUILD_DIR}/lib/clang" "${PACKAGE_DIR}/lib"
cp "${THIS_DIR}/libgcmole.so" "${PACKAGE_DIR}"

# Generate the archive
cd "$(dirname "${PACKAGE_DIR}")"
tar -c -z -f "${PACKAGE_FILE}" "$(basename "${PACKAGE_DIR}")"

# Generate checksum
sha1sum "${PACKAGE_FILE}" | awk '{print $1}' > "${PACKAGE_SUM}"

set +x

echo
echo You can find a packaged version of gcmole here:
echo
echo $(readlink -f "${PACKAGE_FILE}")
echo
echo You can now run gcmole using this command:
echo
echo CLANG_BIN=\"tools/gcmole/gcmole-tools/bin\" python tools/gcmole/gcmole.py
echo
