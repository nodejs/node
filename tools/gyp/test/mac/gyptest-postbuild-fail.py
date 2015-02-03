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
    'xcode': [1, 65],  # 1 for xcode 3, 65 for xcode 4 (see `man sysexits`)
    'make': 2,
    'ninja': 1,
    'xcode-ninja': [1, 65],
  }[test.format]


  # If a postbuild fails, all postbuilds should be re-run on the next build.
  # In Xcode 3, even if the first postbuild fails the other postbuilds were
  # still executed. In Xcode 4, postbuilds are stopped after the first
  # failing postbuild. This test checks for the Xcode 4 behavior.

  # Ignore this test on Xcode 3.
  import subprocess
  job = subprocess.Popen(['xcodebuild', '-version'],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
  out, err = job.communicate()
  if job.returncode != 0:
    print out
    raise Exception('Error %d running xcodebuild' % job.returncode)
  if out.startswith('Xcode 3.'):
    test.pass_test()

  # Non-bundles
  test.build('test.gyp', 'nonbundle', chdir='postbuild-fail',
             status=build_error_code)
  test.built_file_must_not_exist('static_touch',
                                 chdir='postbuild-fail')
  # Check for non-up-to-date-ness by checking if building again produces an
  # error.
  test.build('test.gyp', 'nonbundle', chdir='postbuild-fail',
             status=build_error_code)


  # Bundles
  test.build('test.gyp', 'bundle', chdir='postbuild-fail',
             status=build_error_code)
  test.built_file_must_not_exist('dynamic_touch',
                                 chdir='postbuild-fail')
  # Check for non-up-to-date-ness by checking if building again produces an
  # error.
  test.build('test.gyp', 'bundle', chdir='postbuild-fail',
             status=build_error_code)

  test.pass_test()
