# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Dummy d8 replacement for flaky tests.
"""

# for py2/py3 compatibility
from __future__ import print_function

import os
import sys

PATH = os.path.dirname(os.path.abspath(__file__))

print(' '.join(sys.argv[1:]))

# Test files ending in 'flakes' should first fail then pass. We store state in
# a file side by side with the executable. No clean-up required as all tests
# run in a temp test root. Restriction: Only one variant is supported for now.
for arg in sys.argv[1:]:
  if arg.endswith('flakes'):
    flake_state = os.path.join(PATH, arg)
    if os.path.exists(flake_state):
      sys.exit(0)
    else:
      with open(flake_state, 'w') as f:
        f.write('something')
      sys.exit(1)

sys.exit(0)
