#!/usr/bin/env python

# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies duplicate ldflags are not removed.
"""

import TestGyp

import sys

if sys.platform.startswith('linux'):
  test = TestGyp.TestGyp()

  CHDIR = 'ldflags-duplicates'
  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', chdir=CHDIR)

  test.pass_test()
