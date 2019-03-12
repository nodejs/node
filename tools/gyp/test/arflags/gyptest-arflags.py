#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that building a target with invalid arflags fails.
"""

from __future__ import print_function

import os
import sys
import TestGyp

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)


test = TestGyp.TestGyp(formats=['ninja'])
test.run_gyp('test.gyp')
expected_status = 0 if sys.platform in ['darwin', 'win32'] else 1
test.build('test.gyp', target='lib', status=expected_status)
test.pass_test()
