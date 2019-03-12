#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that list xcode_settings are flattened before being exported to the
environment.
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'non-strs-flattened-to-env'
  INFO_PLIST_PATH = 'Test.app/Contents/Info.plist'

  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)
  info_plist = test.built_file_path(INFO_PLIST_PATH, chdir=CHDIR)
  test.must_exist(info_plist)
  test.must_contain(info_plist, '''\
\t<key>My Variable</key>
\t<string>some expansion</string>''')
  test.must_contain(info_plist, '''\
\t<key>CFlags</key>
\t<string>-fstack-protector-all -fno-strict-aliasing -DS="A Space"</string>''')

  test.pass_test()
