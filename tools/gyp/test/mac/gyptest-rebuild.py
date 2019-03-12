#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are rebuilt correctly.
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'rebuild'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', 'test_app', chdir=CHDIR)

  # Touch a source file, rebuild, and check that the app target is up-to-date.
  test.touch('rebuild/main.c')
  test.build('test.gyp', 'test_app', chdir=CHDIR)

  test.up_to_date('test.gyp', 'test_app', chdir=CHDIR)

  # Xcode runs postbuilds on every build, so targets with postbuilds are
  # never marked as up_to_date.
  if test.format != 'xcode':
    # Same for a framework bundle.
    test.build('test.gyp', 'test_framework_postbuilds', chdir=CHDIR)
    test.up_to_date('test.gyp', 'test_framework_postbuilds', chdir=CHDIR)

    # Test that an app bundle with a postbuild that touches the app binary needs
    # to be built only once.
    test.build('test.gyp', 'test_app_postbuilds', chdir=CHDIR)
    test.up_to_date('test.gyp', 'test_app_postbuilds', chdir=CHDIR)

  test.pass_test()
