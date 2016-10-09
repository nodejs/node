#!/usr/bin/env python
#
# Copyright 2009 the V8 project authors. All rights reserved.
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

# Simple wrapper for running valgrind and checking the output on
# stderr for memory leaks.
# Uses valgrind from third_party/valgrind. Assumes the executable is passed
# with a path relative to the v8 root.


from os import path
import platform
import re
import subprocess
import sys

V8_ROOT = path.dirname(path.dirname(path.abspath(__file__)))
MACHINE = 'linux_x64' if platform.machine() == 'x86_64' else 'linux_x86'
VALGRIND_ROOT = path.join(V8_ROOT, 'third_party', 'valgrind', MACHINE)
VALGRIND_BIN = path.join(VALGRIND_ROOT, 'bin', 'valgrind')
VALGRIND_LIB = path.join(VALGRIND_ROOT, 'lib', 'valgrind')

VALGRIND_ARGUMENTS = [
  VALGRIND_BIN,
  '--error-exitcode=1',
  '--leak-check=full',
  '--smc-check=all',
]

if len(sys.argv) < 2:
  print 'Please provide an executable to analyze.'
  sys.exit(1)

executable = path.join(V8_ROOT, sys.argv[1])
if not path.exists(executable):
  print 'Cannot find the file specified: %s' % executable
  sys.exit(1)

# Compute the command line.
command = VALGRIND_ARGUMENTS + [executable] + sys.argv[2:]

# Run valgrind.
process = subprocess.Popen(
    command,
    stderr=subprocess.PIPE,
    env={'VALGRIND_LIB': VALGRIND_LIB}
)
code = process.wait();
errors = process.stderr.readlines();

# If valgrind produced an error, we report that to the user.
if code != 0:
  sys.stderr.writelines(errors)
  sys.exit(code)

# Look through the leak details and make sure that we don't
# have any definitely, indirectly, and possibly lost bytes.
LEAK_RE = r"(?:definitely|indirectly|possibly) lost: "
LEAK_LINE_MATCHER = re.compile(LEAK_RE)
LEAK_OKAY_MATCHER = re.compile(r"lost: 0 bytes in 0 blocks")
leaks = []
for line in errors:
  if LEAK_LINE_MATCHER.search(line):
    leaks.append(line)
    if not LEAK_OKAY_MATCHER.search(line):
      sys.stderr.writelines(errors)
      sys.exit(1)

# Make sure we found between 2 and 3 leak lines.
if len(leaks) < 2 or len(leaks) > 3:
  sys.stderr.writelines(errors)
  sys.stderr.write('\n\n#### Malformed valgrind output.\n#### Exiting.\n')
  sys.exit(1)

# No leaks found.
sys.stderr.writelines(errors)
sys.exit(0)
