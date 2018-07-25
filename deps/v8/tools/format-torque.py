#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program either generates the parser files for Torque, generating
the source and header files directly in V8's src directory."""

import subprocess
import sys
import re
from subprocess import Popen, PIPE

if len(sys.argv) < 2 or len(sys.argv) > 3:
  print "invalid number of arguments"
  sys.exit(-1)

use_stdout = True
if len(sys.argv) == 3 and sys.argv[1] == '-i':
  use_stdout = False

filename = sys.argv[len(sys.argv) - 1]

with open(filename, 'r') as content_file:
  content = content_file.read()
p = Popen(['clang-format', '-assume-filename=.ts'], stdin=PIPE, stdout=PIPE, stderr=PIPE)
output, err = p.communicate(content)
rc = p.returncode
if (rc <> 0):
  sys.exit(rc);
if use_stdout:
  print output
else:
  output_file = open(filename, 'w')
  output_file.write(output);
  output_file.close()
