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
sys.exit(0)
