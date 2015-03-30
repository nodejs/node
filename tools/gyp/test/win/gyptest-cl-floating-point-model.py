#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure floating point model settings are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp()

  CHDIR = 'compiler-flags'
  test.run_gyp('floating-point-model.gyp', chdir=CHDIR)
  test.build('floating-point-model.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
