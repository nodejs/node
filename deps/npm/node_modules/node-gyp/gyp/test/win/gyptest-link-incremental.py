#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure incremental linking setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('incremental.gyp', chdir=CHDIR)
  test.build('incremental.gyp', test.ALL, chdir=CHDIR)

  def HasILTTables(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    output = test.run_dumpbin('/disasm', full_path)
    return '@ILT+' in output

  # Default or unset is to be on.
  if not HasILTTables('test_incremental_unset.exe'):
    test.fail_test()
  if not HasILTTables('test_incremental_default.exe'):
    test.fail_test()
  if HasILTTables('test_incremental_no.exe'):
    test.fail_test()
  if not HasILTTables('test_incremental_yes.exe'):
    test.fail_test()

  test.pass_test()
