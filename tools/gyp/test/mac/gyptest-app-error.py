#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that invalid strings files cause the build to fail.
"""

import TestCmd
import TestGyp

import sys

if sys.platform == 'darwin':
  expected_error = 'Old-style plist parser: missing semicolon in dictionary'
  saw_expected_error = [False]  # Python2 has no "nonlocal" keyword.
  def match(a, b):
    if a == b:
      return True
    if not TestCmd.is_List(a):
      a = a.split('\n')
    if not TestCmd.is_List(b):
      b = b.split('\n')
    if expected_error in '\n'.join(a) + '\n'.join(b):
      saw_expected_error[0] = True
      return True
    return False
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'], match=match)

  test.run_gyp('test-error.gyp', chdir='app-bundle')

  test.build('test-error.gyp', test.ALL, chdir='app-bundle')

  # Ninja pipes stderr of subprocesses to stdout.
  if test.format in ['ninja', 'xcode-ninja'] \
      and expected_error in test.stdout():
    saw_expected_error[0] = True

  if saw_expected_error[0]:
    test.pass_test()
  else:
    test.fail_test()
