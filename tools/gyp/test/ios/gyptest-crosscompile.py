#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that tools are built correctly.
"""

import TestGyp
import TestMac

import sys
import os

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  oldenv = os.environ.copy()
  try:
    os.environ['GYP_CROSSCOMPILE'] = '1'
    test.run_gyp('test-crosscompile.gyp', chdir='app-bundle')
  finally:
    os.environ.clear()
    os.environ.update(oldenv)

  test.set_configuration('Default')
  test.build('test-crosscompile.gyp', 'TestHost', chdir='app-bundle')
  result_file = test.built_file_path('TestHost', chdir='app-bundle')
  test.must_exist(result_file)
  TestMac.CheckFileType(test, result_file, ['x86_64'])

  test.pass_test()
