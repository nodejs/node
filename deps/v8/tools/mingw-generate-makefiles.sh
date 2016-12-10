#!/bin/sh
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

# Monkey-patch GYP.
cat > tools/gyp/gyp.mingw << EOF
#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

# TODO(mark): sys.path manipulation is some temporary testing stuff.
try:
  import gyp
except ImportError, e:
  import os.path
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), 'pylib'))
  import gyp
  
def MonkeyBuildFileTargets(target_list, build_file):
  """From a target_list, returns the subset from the specified build_file.
  """
  build_file = build_file.replace('/', '\\\\')
  return [p for p in target_list if gyp.common.BuildFile(p) == build_file]
gyp.common.BuildFileTargets = MonkeyBuildFileTargets

import gyp.generator.make
import os
def Monkey_ITIP(self):
  """Returns the location of the final output for an installable target."""
  sep = os.path.sep
  # Xcode puts shared_library results into PRODUCT_DIR, and some gyp files
  # rely on this. Emulate this behavior for mac.
  if (self.type == 'shared_library' and
      (self.flavor != 'mac' or self.toolset != 'target')):
    # Install all shared libs into a common directory (per toolset) for
    # convenient access with LD_LIBRARY_PATH.
    return '\$(builddir)%slib.%s%s%s' % (sep, self.toolset, sep, self.alias)
  return '\$(builddir)' + sep + self.alias
gyp.generator.make.MakefileWriter._InstallableTargetInstallPath = Monkey_ITIP

if __name__ == '__main__':
  sys.exit(gyp.main(sys.argv[1:]))
EOF

# Delete old generated Makefiles.
find out -name '*.mk' -or -name 'Makefile*' -exec rm {} \;

# Generate fresh Makefiles.
mv tools/gyp/gyp tools/gyp/gyp.original
mv tools/gyp/gyp.mingw tools/gyp/gyp
make out/Makefile.ia32
mv tools/gyp/gyp tools/gyp/gyp.mingw
mv tools/gyp/gyp.original tools/gyp/gyp

# Patch generated Makefiles: replace most backslashes with forward slashes,
# fix library names in linker flags.
FILES=$(find out -name '*.mk' -or -name 'Makefile*')
for F in $FILES ; do
  echo "Patching $F..."
  cp $F $F.orig
  cat $F.orig \
    | sed -e 's|\([)a-zA-Z0-9]\)\\\([a-zA-Z]\)|\1/\2|g' \
          -e 's|\([)a-zA-Z0-9]\)\\\\\([a-zA-Z]\)|\1/\2|g' \
          -e 's|'%s/n'|'%s\\\\n'|g' \
          -e 's|-lwinmm\.lib|-lwinmm|g' \
          -e 's|-lws2_32\.lib|-lws2_32|g' \
    > $F
  rm $F.orig
done
