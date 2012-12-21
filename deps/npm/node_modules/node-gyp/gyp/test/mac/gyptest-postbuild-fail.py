#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that a failing postbuild step lets the build fail.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  # set |match| to ignore build stderr output.
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'],
                         match = lambda a, b: True)

  test.run_gyp('test.gyp', chdir='postbuild-fail')

  build_error_code = {
    'xcode': 1,
    'make': 2,
    'ninja': 1,
  }[test.format]


  # If a postbuild fails, all postbuilds should be re-run on the next build.
  # However, even if the first postbuild fails the other postbuilds are still
  # executed.


  # Non-bundles
  test.build('test.gyp', 'nonbundle', chdir='postbuild-fail',
             status=build_error_code)
  test.built_file_must_exist('static_touch',
                             chdir='postbuild-fail')
  # Check for non-up-to-date-ness by checking if building again produces an
  # error.
  test.build('test.gyp', 'nonbundle', chdir='postbuild-fail',
             status=build_error_code)


  # Bundles
  test.build('test.gyp', 'bundle', chdir='postbuild-fail',
             status=build_error_code)
  test.built_file_must_exist('dynamic_touch',
                             chdir='postbuild-fail')
  # Check for non-up-to-date-ness by checking if building again produces an
  # error.
  test.build('test.gyp', 'bundle', chdir='postbuild-fail',
             status=build_error_code)

  test.pass_test()
