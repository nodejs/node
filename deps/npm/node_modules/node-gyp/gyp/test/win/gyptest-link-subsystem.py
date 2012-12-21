#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure subsystem setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('subsystem.gyp', chdir=CHDIR)

  test.build('subsystem.gyp', 'test_console_ok', chdir=CHDIR)
  test.build('subsystem.gyp', 'test_console_fail', chdir=CHDIR, status=1)
  test.build('subsystem.gyp', 'test_windows_ok', chdir=CHDIR)
  test.build('subsystem.gyp', 'test_windows_fail', chdir=CHDIR, status=1)

  # TODO(scottmg): There are other subsystems (WinCE, etc.) that we don't use.

  test.pass_test()
