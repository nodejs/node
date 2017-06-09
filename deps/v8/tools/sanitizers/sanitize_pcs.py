#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Corrects objdump output. The logic is from sancov.py, see comments there."""

import sys;

for line in sys.stdin:
  print '0x%x' % (int(line.strip(), 16) + 4)
