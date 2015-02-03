#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that dylibs can be copied into app bundles.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='copy-dylib')

  test.build('test.gyp', 'test_app', chdir='copy-dylib')

  test.built_file_must_exist(
      'Test App.app/Contents/Resources/libmy_dylib.dylib', chdir='copy-dylib')

  test.pass_test()
