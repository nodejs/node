#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that dependent Xcode settings are processed correctly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['make', 'xcode'])

  CHDIR = 'xcode-env-order'
  INFO_PLIST_PATH = 'Test.app/Contents/Info.plist'

  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)
  info_plist = test.built_file_path(INFO_PLIST_PATH, chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, '>/Source/Project/Test')
  test.must_contain(info_plist, '>DEP:/Source/Project/Test')
  test.must_contain(info_plist,
      '>com.apple.product-type.application:DEP:/Source/Project/Test')

  test.pass_test()
