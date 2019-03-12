#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure nodefaultlib setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('nodefaultlib.gyp', chdir=CHDIR)

  test.build('nodefaultlib.gyp', 'test_ok', chdir=CHDIR)
  test.build('nodefaultlib.gyp', 'test_fail', chdir=CHDIR, status=1)

  test.pass_test()
