#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure tsaware setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('tsaware.gyp', chdir=CHDIR)
  test.build('tsaware.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    return test.run_dumpbin('/headers', test.built_file_path(exe, chdir=CHDIR))

  # Explicitly off, should not be marked NX compatiable.
  if 'Terminal Server Aware' in GetHeaders('test_tsaware_no.exe'):
    test.fail_test()

  # Explicitly on.
  if 'Terminal Server Aware' not in GetHeaders('test_tsaware_yes.exe'):
    test.fail_test()

  test.pass_test()
