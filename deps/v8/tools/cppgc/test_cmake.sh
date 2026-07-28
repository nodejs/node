#!/bin/sh
#
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

sourcedir=$(cd "$(dirname "$0")"; pwd -P)
rootdir=$sourcedir/../../
testdir=$rootdir/test/unittests/

maingn=$rootdir/BUILD.gn
testgn=$testdir/BUILD.gn

function fail {
  echo -e "\033[0;31m$1\033[0m" > /dev/stderr
  exit 1
}

function cleanup {
  rm $cmakelists
  if [[ -d "$tempdir" ]]; then
    rm -rf $tempdir
  fi
}

trap "exit 1" HUP INT PIPE QUIT TERM
trap cleanup EXIT

if [[ ! -f "$maingn" || ! -f "$testgn" ]]; then
  fail "Expected GN files are not present"
fi

cmakelists=$rootdir/CMakeLists.txt

# Generate CMakeLists.txt in the root project directory.
$sourcedir/gen_cmake.py --out=$cmakelists --main-gn=$maingn --test-gn=$testgn
if [ $? -ne 0 ]; then
  fail "CMakeLists.txt generation has failed"
fi

# Create a temporary build directory.
tempdir=$(mktemp -d)
if [[ ! "$tempdir" || ! -d "$tempdir" ]]; then
  fail "Failed to create temporary dir"
fi

# Configure project with cmake.
cd $tempdir
cmake -GNinja $rootdir || fail "Failed to execute cmake"

# Build all targets.
ninja cppgc || fail "Failed to build cppgc"
ninja cppgc_hello_world || fail "Failed to build sample"
ninja cppgc_unittests || fail "Failed to build unittests"

# Run unittests.
./cppgc_unittests || fail "Failed to run unittests"

echo -e "\033[0;32mThe test has succesfully passed\033[0m"
