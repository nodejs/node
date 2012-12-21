#!/usr/bin/env python
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Takes 3 arguments. Writes the 1st argument to the file in the 2nd argument,
# and writes the absolute path to the file in the 2nd argument to the file in
# the 3rd argument.

import os
import shlex
import sys

if len(sys.argv) == 3 and ' ' in sys.argv[2]:
  sys.argv[2], fourth = shlex.split(sys.argv[2].replace('\\', '\\\\'))
  sys.argv.append(fourth)

#print >>sys.stderr, sys.argv

with open(sys.argv[2], 'w') as f:
  f.write(sys.argv[1])

with open(sys.argv[3], 'w') as f:
  f.write(os.path.abspath(sys.argv[2]))
