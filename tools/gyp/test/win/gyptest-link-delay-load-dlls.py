#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure delay load setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('delay-load-dlls.gyp', chdir=CHDIR)
  test.build('delay-load-dlls.gyp', test.ALL, chdir=CHDIR)

  prefix = 'contains the following delay load imports:'
  shell32_look_for = prefix + '\r\n\r\n    SHELL32.dll'

  output = test.run_dumpbin(
      '/all', test.built_file_path('test_dld_none.exe', chdir=CHDIR))
  if prefix in output:
    test.fail_test()

  output = test.run_dumpbin(
      '/all', test.built_file_path('test_dld_shell32.exe', chdir=CHDIR))
  if shell32_look_for not in output:
    test.fail_test()

  test.pass_test()
