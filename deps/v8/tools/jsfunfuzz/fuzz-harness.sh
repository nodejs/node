#!/bin/bash
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple harness that downloads and runs 'jsfunfuzz' against d8. This
# takes a long time because it runs many iterations and is intended for
# automated usage. The package containing 'jsfunfuzz' can be found as an
# attachment to this bug:
# https://bugzilla.mozilla.org/show_bug.cgi?id=jsfunfuzz

JSFUNFUZZ_URL="https://bugzilla.mozilla.org/attachment.cgi?id=310631"
JSFUNFUZZ_MD5="d0e497201c5cd7bffbb1cdc1574f4e32"

v8_root=$(readlink -f $(dirname $BASH_SOURCE)/../../)
jsfunfuzz_dir="$v8_root/tools/jsfunfuzz"
cd "$jsfunfuzz_dir"

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
@@ -118,19 +118,19 @@
-def showtail(logfilename):
+def showtail(logfilename, method="tail"):
-   cmd = "tail -n 20 %s" % logfilename
+   cmd = "%s -n 20 %s" % (method, logfilename)
    print cmd
    print ""
    os.system(cmd)
    print ""
    print ""

 def many_timed_runs():
     iteration = 0
-    while True:
+    while iteration < 100:
         iteration += 1
         logfilename = "w%d" % iteration
         one_timed_run(logfilename)
         if not succeeded(logfilename):
             showtail(logfilename)
-            showtail("err-" + logfilename)
+            showtail("err-" + logfilename, method="head")

             many_timed_runs()
EOF
  fi

fi

flags='--expose-gc --verify-gc'
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
