#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure reference optimization setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('opt-ref.gyp', chdir=CHDIR)
  test.build('opt-ref.gyp', chdir=CHDIR)

  # We're specifying /DEBUG so the default is to not remove unused functions.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_optref_default.exe', chdir=CHDIR))
  if 'unused_function' not in output:
    test.fail_test()

  # Explicitly off, unused_function preserved.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_optref_no.exe', chdir=CHDIR))
  if 'unused_function' not in output:
    test.fail_test()

  # Explicitly on, should be removed.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_optref_yes.exe', chdir=CHDIR))
  if 'unused_function' in output:
    test.fail_test()

  test.pass_test()
