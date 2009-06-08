#!/usr/bin/python
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

import subprocess
import sys
import re

VALGRIND_ARGUMENTS = [
  'valgrind',
  '--error-exitcode=1',
  '--leak-check=full',
  '--smc-check=all'
]

# Compute the command line.
command = VALGRIND_ARGUMENTS + sys.argv[1:]

# Run valgrind.
process = subprocess.Popen(command, stderr=subprocess.PIPE)
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
LEAK_OKAY_MATCHER = re.compile(r"lost: 0 bytes in 0 blocks.")
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
sys.exit(0)
