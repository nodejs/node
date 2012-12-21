#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure comdat folding optimization setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('opt-icf.gyp', chdir=CHDIR)
  test.build('opt-icf.gyp', chdir=CHDIR)

  # We're specifying /DEBUG so the default is to not merge identical
  # functions, so all of the similar_functions should be preserved.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_opticf_default.exe', chdir=CHDIR))
  if output.count('similar_function') != 6: # 3 definitions, 3 calls.
    test.fail_test()

  # Explicitly off, all functions preserved seperately.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_opticf_no.exe', chdir=CHDIR))
  if output.count('similar_function') != 6: # 3 definitions, 3 calls.
    test.fail_test()

  # Explicitly on, all but one removed.
  output = test.run_dumpbin(
      '/disasm', test.built_file_path('test_opticf_yes.exe', chdir=CHDIR))
  if output.count('similar_function') != 4: # 1 definition, 3 calls.
    test.fail_test()

  test.pass_test()
