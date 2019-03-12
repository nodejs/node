#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure exception handling settings are extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('exception-handling.gyp', chdir=CHDIR)

  # Must fail.
  test.build('exception-handling.gyp', 'test_eh_off', chdir=CHDIR,
      status=1)

  # Must succeed.
  test.build('exception-handling.gyp', 'test_eh_s', chdir=CHDIR)
  test.build('exception-handling.gyp', 'test_eh_a', chdir=CHDIR)

  # Error code must be 1 if EHa, and 2 if EHsc.
  test.run_built_executable('test_eh_a', chdir=CHDIR, status=1)
  test.run_built_executable('test_eh_s', chdir=CHDIR, status=2)

  test.pass_test()
