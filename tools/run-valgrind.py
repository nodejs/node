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

from os import path
import subprocess
import sys

NODE_ROOT = path.dirname(path.dirname(path.abspath(__file__)))

VALGRIND_ARGUMENTS = [
  'valgrind',
  '--error-exitcode=1',
  '--smc-check=all',
  # Node.js does not clean up on exit so don't complain about
  # memory leaks but do complain about invalid memory access.
  '--quiet',
]

if len(sys.argv) < 2:
  print 'Please provide an executable to analyze.'
  sys.exit(1)

executable = path.join(NODE_ROOT, sys.argv[1])
if not path.exists(executable):
  print 'Cannot find the file specified: %s' % executable
  sys.exit(1)

# Compute the command line.
command = VALGRIND_ARGUMENTS + [executable] + sys.argv[2:]

# Run valgrind.
process = subprocess.Popen(command, stderr=subprocess.PIPE)
code = process.wait()
errors = process.stderr.readlines()

# If valgrind produced an error, we report that to the user.
if code != 0:
  sys.stderr.writelines(errors)
  sys.exit(code)
