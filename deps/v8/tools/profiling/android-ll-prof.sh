#!/bin/bash
# Copyright 2012 the V8 project authors. All rights reserved.
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

# Runs d8 with the given arguments on the device under 'perf' and
# processes the profiler trace and v8 logs using ll_prof.py.
# 
# Usage:
# > ./tools/android-ll-prof.sh (debug|release) "args to d8" "args to ll_prof.py"
#
# The script creates deploy directory deploy/data/local/tmp/v8, copies there
# the d8 binary either from out/android_arm.release or out/android_arm.debug,
# and then sync the deploy directory with /data/local/tmp/v8 on the device.
# You can put JS files in the deploy directory before running the script.
# Note: $ANDROID_NDK_ROOT must be set.

MODE=$1
RUN_ARGS=$2
LL_PROF_ARGS=$3

BASE=`cd $(dirname "$0")/..; pwd`
DEPLOY="$BASE/deploy"

set +e
mkdir -p "$DEPLOY/data/local/tmp/v8"

cp "$BASE/out/android_arm.$MODE/d8" "$DEPLOY/data/local/tmp/v8/d8"

adb -p "$DEPLOY" sync data

adb shell "cd /data/local/tmp/v8;\
           perf record -R -e cycles -c 10000 -f -i \
           ./d8 --ll_prof --gc-fake-mmap=/data/local/tmp/__v8_gc__ $RUN_ARGS"

adb pull /data/local/tmp/v8/v8.log .
adb pull /data/local/tmp/v8/v8.log.ll .
adb pull /data/perf.data .

ARCH=arm-linux-androideabi-4.6
TOOLCHAIN="${ANDROID_NDK_ROOT}/toolchains/$ARCH/prebuilt/linux-x86/bin"

$BASE/tools/ll_prof.py --host-root="$BASE/deploy" \
                       --gc-fake-mmap=/data/local/tmp/__v8_gc__ \
                       --objdump="$TOOLCHAIN/arm-linux-androideabi-objdump" \
                       $LL_PROF_ARGS
