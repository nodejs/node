#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the global xcode_settings processing doesn't throw.
Regression test for http://crbug.com/109163
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

  test.run_gyp('src/dir2/dir2.gyp', chdir='global-settings', depth='src')
  # run_gyp shouldn't throw.

  # Check that BUILT_PRODUCTS_DIR was set correctly, too.
  test.build('dir2/dir2.gyp', 'dir2_target', chdir='global-settings/src',
             SYMROOT='../build')
  test.built_file_must_exist('file.txt', chdir='global-settings/src')

  test.pass_test()
