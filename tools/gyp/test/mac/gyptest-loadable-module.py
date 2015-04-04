#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests that a loadable_module target is built correctly.
"""

import TestGyp

import os
import struct
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'loadable-module'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  # Binary.
  binary = test.built_file_path(
      'test_loadable_module.plugin/Contents/MacOS/test_loadable_module',
      chdir=CHDIR)
  test.must_exist(binary)
  MH_BUNDLE = 8
  if struct.unpack('4I', open(binary, 'rb').read(16))[3] != MH_BUNDLE:
    test.fail_test()

  # Info.plist.
  info_plist = test.built_file_path(
      'test_loadable_module.plugin/Contents/Info.plist', chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, """
	<key>CFBundleExecutable</key>
	<string>test_loadable_module</string>
""")

  # PkgInfo.
  test.built_file_must_not_exist(
      'test_loadable_module.plugin/Contents/PkgInfo', chdir=CHDIR)
  test.built_file_must_not_exist(
      'test_loadable_module.plugin/Contents/Resources', chdir=CHDIR)

  test.pass_test()
