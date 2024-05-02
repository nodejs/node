#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Compare builtins hashes from two architectures and enforce their equality.
Bail out with an error and details on the difference otherwise.
"""

import difflib
import sys

assert len(sys.argv) > 3

file1 = sys.argv[1]
file2 = sys.argv[2]
report = sys.argv[3]

with open(file1) as f1, open(file2) as f2:
  diff = list(difflib.unified_diff(
      f1.read().splitlines(),
      f2.read().splitlines(),
      fromfile=file1,
      tofile=file2,
      lineterm='',
  ))

if diff:
  print('Detected incompatible builtins hashes:')
  for line in diff:
    print(line)
  sys.exit(1)

with open(report, 'w') as f:
  f.write('No builtins incompatibilities detected.')
