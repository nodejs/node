#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure the /safeseh option can be passed to ml.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  CHDIR = 'ml-safeseh'
  test.run_gyp('ml-safeseh.gyp', chdir=CHDIR)
  test.build('ml-safeseh.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
