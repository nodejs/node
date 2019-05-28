#!/usr/bin/env bash

# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script will build libgcmole.so as well as a corresponding recent
# version of Clang and LLVM. The Clang will be built with the locally
# installed compiler and statically link against the local libstdc++ so
# that the resulting binary is easier transferable between different
# environments.

CLANG_RELEASE=8.0

THIS_DIR="$(readlink -f "$(dirname "${0}")")"
LLVM_DIR="${THIS_DIR}/../../third_party/llvm"
CLANG_DIR="${THIS_DIR}/../../third_party/clang"
BUILD_DIR="${THIS_DIR}/../../third_party/llvm+clang-build"

LLVM_REPO_URL=${LLVM_URL:-https://llvm.org/svn/llvm-project}

# Die if any command dies.
set -e

OS="$(uname -s)"

# Xcode and clang don't get along when predictive compilation is enabled.
# http://crbug.com/96315
if [[ "${OS}" = "Darwin" ]] && xcodebuild -version | grep -q 'Xcode 3.2' ; then
  XCONF=com.apple.Xcode
  if [[ "${GYP_GENERATORS}" != "make" ]] && \
     [ "$(defaults read "${XCONF}" EnablePredictiveCompilation)" != "0" ]; then
    echo
    echo "          HEARKEN!"
    echo "You're using Xcode3 and you have 'Predictive Compilation' enabled."
    echo "This does not work well with clang (http://crbug.com/96315)."
    echo "Disable it in Preferences->Building (lower right), or run"
    echo "    defaults write ${XCONF} EnablePredictiveCompilation -boolean NO"
    echo "while Xcode is not running."
    echo
  fi

  SUB_VERSION=$(xcodebuild -version | sed -Ene 's/Xcode 3\.2\.([0-9]+)/\1/p')
  if [[ "${SUB_VERSION}" < 6 ]]; then
    echo
    echo "          YOUR LD IS BUGGY!"
    echo "Please upgrade Xcode to at least 3.2.6."
    echo
  fi
fi

echo Getting LLVM release "${CLANG_RELEASE}" in "${LLVM_DIR}"
if ! svn co --force \
    "${LLVM_REPO_URL}/llvm/branches/release_${CLANG_RELEASE/./}" \
    "${LLVM_DIR}"; then
  echo Checkout failed, retrying
  rm -rf "${LLVM_DIR}"
  svn co --force \
      "${LLVM_REPO_URL}/llvm/branches/release_${CLANG_RELEASE/./}" \
      "${LLVM_DIR}"
fi

echo Getting clang release "${CLANG_RELEASE}" in "${CLANG_DIR}"
svn co --force \
    "${LLVM_REPO_URL}/cfe/branches/release_${CLANG_RELEASE/./}" \
    "${CLANG_DIR}"

# Echo all commands
set -x

NUM_JOBS=3
if [[ "${OS}" = "Linux" ]]; then
  NUM_JOBS="$(grep -c "^processor" /proc/cpuinfo)"
elif [ "${OS}" = "Darwin" ]; then
  NUM_JOBS="$(sysctl -n hw.ncpu)"
fi

# Build clang.
if [ ! -e "${BUILD_DIR}" ]; then
  mkdir "${BUILD_DIR}"
fi
cd "${BUILD_DIR}"
cmake -DCMAKE_CXX_FLAGS="-static-libstdc++" -DLLVM_ENABLE_TERMINFO=OFF \
    -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS=clang "${LLVM_DIR}"
MACOSX_DEPLOYMENT_TARGET=10.5 make -j"${NUM_JOBS}"

# Strip the clang binary.
STRIP_FLAGS=
if [ "${OS}" = "Darwin" ]; then
  # See http://crbug.com/256342
  STRIP_FLAGS=-x
fi
strip ${STRIP_FLAGS} bin/clang
cd -

# Build libgcmole.so
make -C "${THIS_DIR}" clean
make -C "${THIS_DIR}" LLVM_SRC_ROOT="${LLVM_DIR}" \
    CLANG_SRC_ROOT="${CLANG_DIR}" BUILD_ROOT="${BUILD_DIR}" libgcmole.so

set +x

echo
echo You can now run gcmole using this command:
echo
echo CLANG_BIN=\"third_party/llvm+clang-build/bin\" lua tools/gcmole/gcmole.lua
echo
