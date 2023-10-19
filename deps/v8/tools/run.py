#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program wraps an arbitrary command since gn currently can only execute
scripts."""

import subprocess
import sys

result = subprocess.call(sys.argv[1:])
if result != 0:
  # Windows error codes such as 0xC0000005 and 0xC0000409 are much easier
  # to recognize and differentiate in hex.
  if result < -100:
    # Print negative hex numbers as positive by adding 2^32.
    print('Return code is %08X' % (result + 2**32))
  else:
    print('Return code is %d' % result)
sys.exit(result)
