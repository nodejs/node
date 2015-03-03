#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that CLANG_CXX_LIBRARY works.
"""

import TestGyp
import TestMac

import os
import sys

if sys.platform == 'darwin':
  # Xcode 4.2 on OS X 10.6 doesn't install the libc++ headers, don't run this
  # test there.
  if TestMac.Xcode.Version() <= '0420':
    sys.exit(0)

  test = TestGyp.TestGyp(formats=['make', 'ninja', 'xcode'])
  test.run_gyp('clang-cxx-library.gyp', chdir='clang-cxx-library')
  test.build('clang-cxx-library.gyp', test.ALL, chdir='clang-cxx-library')

  test.pass_test()

