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

# A simple harness that downloads and runs 'jsfunfuzz' against d8. This
# takes a long time because it runs many iterations and is intended for
# automated usage. The package containing 'jsfunfuzz' can be found as an
# attachment to this bug:
# https://bugzilla.mozilla.org/show_bug.cgi?id=jsfunfuzz

JSFUNFUZZ_URL="https://bugzilla.mozilla.org/attachment.cgi?id=310631"
JSFUNFUZZ_MD5="d0e497201c5cd7bffbb1cdc1574f4e32"

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../)
jsfunfuzz_dir="$v8_root/tools/jsfunfuzz"

if [ -n "$1" ]; then
  d8="${v8_root}/$1"
else
  d8="${v8_root}/d8"
fi

if [ ! -f "$d8" ]; then
  echo "Failed to find d8 binary: $d8"
  exit 1
fi

# Deprecated download method. A prepatched archive is downloaded as a hook
# if jsfunfuzz=1 is specified as a gyp flag. Requires google.com authentication
# for google storage.
if [ "$3" == "--download" ]; then

  jsfunfuzz_file="$v8_root/tools/jsfunfuzz.zip"
  if [ ! -f "$jsfunfuzz_file" ]; then
    echo "Downloading $jsfunfuzz_file ..."
    wget -q -O "$jsfunfuzz_file" $JSFUNFUZZ_URL || exit 1
  fi

  jsfunfuzz_sum=$(md5sum "$jsfunfuzz_file" | awk '{ print $1 }')
  if [ $jsfunfuzz_sum != $JSFUNFUZZ_MD5 ]; then
    echo "Failed to verify checksum!"
    exit 1
  fi

  if [ ! -d "$jsfunfuzz_dir" ]; then
    echo "Unpacking into $jsfunfuzz_dir ..."
    unzip "$jsfunfuzz_file" -d "$jsfunfuzz_dir" || exit 1
    echo "Patching runner ..."
    cat << EOF | patch -s -p0 -d "$v8_root"
--- tools/jsfunfuzz/jsfunfuzz/multi_timed_run.py~
+++ tools/jsfunfuzz/jsfunfuzz/multi_timed_run.py
@@ -125,7 +125,7 @@
 
 def many_timed_runs():
     iteration = 0
-    while True:
+    while iteration < 100:
         iteration += 1
         logfilename = "w%d" % iteration
         one_timed_run(logfilename)
EOF
  fi

fi

flags='--debug-code --expose-gc --verify-gc'
python -u "$jsfunfuzz_dir/jsfunfuzz/multi_timed_run.py" 300 \
    "$d8" $flags "$jsfunfuzz_dir/jsfunfuzz/jsfunfuzz.js"
exit_code=$(cat w* | grep " looking good" -c)
exit_code=$((100-exit_code))

if [ -n "$2" ]; then
  archive="$2"
else
  archive=fuzz-results-$(date +%Y%m%d%H%M%S).tar.bz2
fi
echo "Creating archive $archive"
tar -cjf $archive err-* w*
rm -f err-* w*

echo "Total failures: $exit_code"
exit $exit_code
