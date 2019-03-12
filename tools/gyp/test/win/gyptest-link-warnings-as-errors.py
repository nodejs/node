#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure linker warnings-as-errors setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('warn-as-error.gyp', chdir=CHDIR)

  test.build('warn-as-error.gyp', 'test_on', chdir=CHDIR, status=1)
  test.build('warn-as-error.gyp', 'test_off', chdir=CHDIR)
  test.build('warn-as-error.gyp', 'test_default', chdir=CHDIR)
  test.pass_test()
