#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure ForceSymbolReference is translated properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('force-symbol-reference.gyp', chdir=CHDIR)
  test.build('force-symbol-reference.gyp', test.ALL, chdir=CHDIR)

  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_force_reference.exe', chdir=CHDIR))
  if '?x@@YAHXZ:' not in output or '?y@@YAHXZ:' not in output:
    test.fail_test()
  test.pass_test()
