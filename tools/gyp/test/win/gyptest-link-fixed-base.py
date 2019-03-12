#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure fixed base setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('fixed-base.gyp', chdir=CHDIR)
  test.build('fixed-base.gyp', test.ALL, chdir=CHDIR)

  def GetHeaders(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    return test.run_dumpbin('/headers', full_path)

  # For exe, default is fixed, for dll, it's not fixed.
  if 'Relocations stripped' not in GetHeaders('test_fixed_default_exe.exe'):
    test.fail_test()
  if 'Relocations stripped' in GetHeaders('test_fixed_default_dll.dll'):
    test.fail_test()

  # Explicitly not fixed.
  if 'Relocations stripped' in GetHeaders('test_fixed_no.exe'):
    test.fail_test()

  # Explicitly fixed.
  if 'Relocations stripped' not in GetHeaders('test_fixed_yes.exe'):
    test.fail_test()

  test.pass_test()
