#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure additional include dirs are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('additional-include-dirs.gyp', chdir=CHDIR)
  test.build('additional-include-dirs.gyp', test.ALL, chdir=CHDIR)

  test.pass_test()
