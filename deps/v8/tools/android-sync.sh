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

# This script pushes android binaries and test data to the device.
# The first argument can be either "android.release" or "android.debug".
# The second argument is a relative path to the output directory with binaries.
# The third argument is the absolute path to the V8 directory on the host.
# The fourth argument is the absolute path to the V8 directory on the device.

if [ ${#@} -lt 4 ] ; then
  echo "$0: Error: need 4 arguments"
  exit 1
fi

ARCH_MODE=$1
OUTDIR=$2
HOST_V8=$3
ANDROID_V8=$4

function LINUX_MD5 {
  local HASH=$(md5sum $1)
  echo ${HASH%% *}
}

function DARWIN_MD5 {
  local HASH=$(md5 $1)
  echo ${HASH} | cut -f2 -d "=" | cut -f2 -d " "
}

host_os=$(uname -s)
case "${host_os}" in
  "Linux")
    MD5=LINUX_MD5
    ;;
  "Darwin")
    MD5=DARWIN_MD5
    ;;
  *)
    echo "$0: Host platform ${host_os} is not supported" >& 2
    exit 1
esac

function sync_file {
  local FILE=$1
  local ANDROID_HASH=$(adb shell "md5 \"$ANDROID_V8/$FILE\"")
  local HOST_HASH=$($MD5 "$HOST_V8/$FILE")
  if [ "${ANDROID_HASH%% *}" != "${HOST_HASH}" ]; then
    adb push "$HOST_V8/$FILE" "$ANDROID_V8/$FILE" &> /dev/null
  fi
  echo -n "."
}

function sync_dir {
  local DIR=$1
  echo -n "sync to $ANDROID_V8/$DIR"
  for FILE in $(find "$HOST_V8/$DIR" -not -path "*.svn*" -type f); do
    local RELATIVE_FILE=${FILE:${#HOST_V8}}
    sync_file "$RELATIVE_FILE"
  done
  echo ""
}

echo -n "sync to $ANDROID_V8/$OUTDIR/$ARCH_MODE"
sync_file "$OUTDIR/$ARCH_MODE/cctest"
sync_file "$OUTDIR/$ARCH_MODE/d8"
sync_file "$OUTDIR/$ARCH_MODE/preparser"
echo ""
echo -n "sync to $ANDROID_V8/tools"
sync_file tools/consarray.js
sync_file tools/codemap.js
sync_file tools/csvparser.js
sync_file tools/profile.js
sync_file tools/splaytree.js
sync_file tools/profile_view.js
sync_file tools/logreader.js
sync_file tools/tickprocessor.js
echo ""
sync_dir test/message
sync_dir test/mjsunit
sync_dir test/preparser
