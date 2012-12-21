#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests things related to ARCHS.
"""

import TestGyp

import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  def CheckFileType(file, expected):
    proc = subprocess.Popen(['file', '-b', file], stdout=subprocess.PIPE)
    o = proc.communicate()[0].strip()
    assert not proc.returncode
    if o != expected:
      print 'File: Expected %s, got %s' % (expected, o)
      test.fail_test()

  test.run_gyp('test-no-archs.gyp', chdir='archs')
  test.build('test-no-archs.gyp', test.ALL, chdir='archs')
  result_file = test.built_file_path('Test', chdir='archs')
  test.must_exist(result_file)
  CheckFileType(result_file, 'Mach-O executable i386')

  test.run_gyp('test-archs-x86_64.gyp', chdir='archs')
  test.build('test-archs-x86_64.gyp', test.ALL, chdir='archs')
  result_file = test.built_file_path('Test64', chdir='archs')
  test.must_exist(result_file)
  CheckFileType(result_file, 'Mach-O 64-bit executable x86_64')
