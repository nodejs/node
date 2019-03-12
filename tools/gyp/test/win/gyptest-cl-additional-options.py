#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure additional manual compiler flags are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('additional-options.gyp', chdir=CHDIR)

  # Warning level not overidden, must fail.
  test.build('additional-options.gyp', 'test_additional_none', chdir=CHDIR,
      status=1)

  # Warning level is overridden, must succeed.
  test.build('additional-options.gyp', 'test_additional_one', chdir=CHDIR)

  test.pass_test()
