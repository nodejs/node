#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure nxcompat setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('nxcompat.gyp', chdir=CHDIR)
  test.build('nxcompat.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    return test.run_dumpbin('/headers', test.built_file_path(exe, chdir=CHDIR))

  # NXCOMPAT is on by default.
  if 'NX compatible' not in GetHeaders('test_nxcompat_default.exe'):
    test.fail_test()

  # Explicitly off, should not be marked NX compatiable.
  if 'NX compatible' in GetHeaders('test_nxcompat_no.exe'):
    test.fail_test()

  # Explicitly on.
  if 'NX compatible' not in GetHeaders('test_nxcompat_yes.exe'):
    test.fail_test()

  test.pass_test()
