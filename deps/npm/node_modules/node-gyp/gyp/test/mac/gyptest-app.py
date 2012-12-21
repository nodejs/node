#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are built correctly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='app-bundle')

  test.build('test.gyp', test.ALL, chdir='app-bundle')

  # Binary
  test.built_file_must_exist('Test App Gyp.app/Contents/MacOS/Test App Gyp',
                             chdir='app-bundle')

  # Info.plist
  info_plist = test.built_file_path('Test App Gyp.app/Contents/Info.plist',
                                    chdir='app-bundle')
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'com.google.Test App Gyp')  # Variable expansion

  # Resources
  test.built_file_must_exist(
      'Test App Gyp.app/Contents/Resources/English.lproj/InfoPlist.strings',
      chdir='app-bundle')
  test.built_file_must_exist(
      'Test App Gyp.app/Contents/Resources/English.lproj/MainMenu.nib',
      chdir='app-bundle')

  # Packaging
  test.built_file_must_exist('Test App Gyp.app/Contents/PkgInfo',
                             chdir='app-bundle')
  test.built_file_must_match('Test App Gyp.app/Contents/PkgInfo', 'APPLause',
                             chdir='app-bundle')


  test.pass_test()
