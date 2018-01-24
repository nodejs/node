#!/bin/bash
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build and collect code coverage data, cummulatively, on specified architectures.

BUILD_TYPE=${BUILD_TYPE:-Release}

declare -A modes=( [Release]=release [Debug]=debug )
declare -A pairs=( [arm]=ia32 [arm64]=x64 [ia32]=ia32 [x64]=x64 )

if [ -z ${modes[$BUILD_TYPE]} ]
then
    echo "BUILD_TYPE must be {<unspecified>|Release|Debug}"
    echo "Release is default"
    exit
fi

mode=${modes[$BUILD_TYPE]}

echo "Using build:" $BUILD_TYPE
v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
work_dir=$v8_root/cov
build_dir=$work_dir/$BUILD_TYPE

if [ -z $@ ]
then
    echo "Pass at least one target architecture"
    echo "Supported architectures: x64 ia32 arm arm64"
    echo ""
    echo "Example: ./tools/gcov.sh x64 arm"
    echo ""
    echo "Optionally, set BUILD_TYPE env variable to"
    echo "either Debug or Release, to use the corresponding build."
    echo "By default, BUILD_TYPE is Release."
    echo ""
    echo "Example: BUILD_TYPE=Debug ./tools/gcov.sh x64 arm"
    echo ""
    exit
fi

lcov --directory=$build_dir --zerocounters

# Mapping v8 build terminology to gnu compiler terminology:
#   target_arch is the host, and
#   v8_target_arch is the target

for v8_target_arch in "$@"
do
    target_arch=${pairs[$v8_target_arch]}
    if [ -z $target_arch ]
    then
        echo "Skipping unknown architecture: " $v8_target_arch
    else
        echo "Building" $v8_target_arch
        GYP_DEFINES="component=static_library use_goma=1 target_arch=$target_arch v8_target_arch=$v8_target_arch coverage=1 clang=0" python $v8_root/gypfiles/gyp_v8.py -G output_dir=$work_dir
        ninja -C $build_dir -j2000
        $v8_root/tools/run-tests.py --gcov-coverage --arch=$v8_target_arch --mode=$mode --shell-dir=$build_dir --variants=exhaustive
    fi
done

lcov --directory=$build_dir --capture --output-file $work_dir/app.info
genhtml --output-directory $work_dir/html $work_dir/app.info
echo "Done"
echo "Output available at: " $work_dir/html/index.html
