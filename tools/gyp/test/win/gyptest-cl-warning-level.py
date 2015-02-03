#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure warning level is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('warning-level.gyp', chdir=CHDIR)

  # A separate target for each warning level: one pass (compiling a file
  # containing a warning that's above the specified level); and one fail
  # (compiling a file at the specified level). No pass for 4 of course,
  # because it would have to have no warnings. The default warning level is
  # equivalent to level 1.

  test.build('warning-level.gyp', 'test_wl1_fail', chdir=CHDIR, status=1)
  test.build('warning-level.gyp', 'test_wl1_pass', chdir=CHDIR)

  test.build('warning-level.gyp', 'test_wl2_fail', chdir=CHDIR, status=1)
  test.build('warning-level.gyp', 'test_wl2_pass', chdir=CHDIR)

  test.build('warning-level.gyp', 'test_wl3_fail', chdir=CHDIR, status=1)
  test.build('warning-level.gyp', 'test_wl3_pass', chdir=CHDIR)

  test.build('warning-level.gyp', 'test_wl4_fail', chdir=CHDIR, status=1)

  test.build('warning-level.gyp', 'test_def_fail', chdir=CHDIR, status=1)
  test.build('warning-level.gyp', 'test_def_pass', chdir=CHDIR)

  test.pass_test()
