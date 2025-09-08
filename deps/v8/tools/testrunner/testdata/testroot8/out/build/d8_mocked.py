# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Dummy d8 replacement.
"""

import sys

args = ' '.join(sys.argv[1:])
print(args)

# Test numfuzz behavior: the bananas test requires --fuzzing.
if 'bananas' in args and '--fuzzing' not in args:
  sys.exit(1)

sys.exit(0)
