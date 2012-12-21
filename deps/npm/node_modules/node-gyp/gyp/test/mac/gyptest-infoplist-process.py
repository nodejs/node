#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies the Info.plist preprocessor functionality.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'infoplist-process'
  INFO_PLIST_PATH = 'Test.app/Contents/Info.plist'

  # First process both keys.
  test.set_configuration('One')
  test.run_gyp('test1.gyp', chdir=CHDIR)
  test.build('test1.gyp', test.ALL, chdir=CHDIR)
  info_plist = test.built_file_path(INFO_PLIST_PATH, chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'Foo')
  test.must_contain(info_plist, 'Bar')

  # Then process a single key.
  test.set_configuration('Two')
  test.run_gyp('test2.gyp', chdir=CHDIR)
  test.build('test2.gyp', chdir=CHDIR)
  info_plist = test.built_file_path(INFO_PLIST_PATH, chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'com.google.Test')  # Normal expansion works.
  test.must_contain(info_plist, 'Foo (Bar)')
  test.must_contain(info_plist, 'PROCESSED_KEY2')

  # Then turn off the processor.
  test.set_configuration('Three')
  test.run_gyp('test3.gyp', chdir=CHDIR)
  test.build('test3.gyp', chdir=CHDIR)
  info_plist = test.built_file_path('Test App.app/Contents/Info.plist',
                                    chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'com.google.Test')  # Normal expansion works.
  test.must_contain(info_plist, 'PROCESSED_KEY1')
  test.must_contain(info_plist, 'PROCESSED_KEY2')

  test.pass_test()
