# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Dummy d8 replacement. Just passes all test, except if 'berries' is in args.
"""

# for py2/py3 compatibility
from __future__ import print_function

import sys

args = ' '.join(sys.argv[1:])
print(args)

# Let all berries fail.
if 'berries' in args:
  sys.exit(1)

# Dummy results if some analysis flags for numfuzz are present.
if '--print-deopt-stress' in args:
  print('=== Stress deopt counter: 12345')

if '--fuzzer-gc-analysis' in args:
  print('### Maximum marking limit reached = 3.70')
  print('### Maximum new space size reached = 7.30')
  print('### Allocations = 9107, hash = 0x0000001b')

sys.exit(0)
