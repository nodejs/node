#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies libraries (in link_settings) are properly found.
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

  test.skip_test()  # bug=535

  test.run_gyp('subdir/test.gyp', chdir='libraries')

  test.build('subdir/test.gyp', test.ALL, chdir='libraries')

  test.pass_test()
