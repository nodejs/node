#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that env vars work with actions, with relative directory paths.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  # The xcode-ninja generator handles gypfiles which are not at the
  # project root incorrectly.
  # cf. https://code.google.com/p/gyp/issues/detail?id=460
  if test.format == 'xcode-ninja':
    test.skip_test()

  CHDIR = 'action-envvars'
  test.run_gyp('action/action.gyp', chdir=CHDIR)
  test.build('action/action.gyp', 'action', chdir=CHDIR, SYMROOT='../build')

  result_file = test.built_file_path('result', chdir=CHDIR)
  test.must_exist(result_file)
  test.must_contain(result_file, 'Test output')

  other_result_file = test.built_file_path('other_result', chdir=CHDIR)
  test.must_exist(other_result_file)
  test.must_contain(other_result_file, 'Other output')

  test.pass_test()
