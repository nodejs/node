#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

assert len(sys.argv) == 3

if sys.argv[1] == 'equal':
  # 1. Scenario: print equal allocation hashes.
  print '### Allocations = 9497, hash = 0xc322c6b0'
elif sys.argv[1] == 'differ':
  # 2. Scenario: print different allocation hashes. This prints a different
  # hash on the second run, based on the content of a semaphore file. This
  # file is expected to be empty in the beginning.
  with open(sys.argv[2]) as f:
    if f.read():
      print '### Allocations = 9497, hash = 0xc322c6b0'
    else:
      print '### Allocations = 9497, hash = 0xc322c6b1'
  with open(sys.argv[2], 'w') as f:
    f.write('something')
else:
  # 3. Scenario: missing allocation hashes. Don't print anything.
  assert 'missing'

sys.exit(0)
