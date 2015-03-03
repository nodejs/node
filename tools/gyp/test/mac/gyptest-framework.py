#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are built correctly.
"""

import TestGyp

import os
import sys


def ls(path):
  '''Returns a list of all files in a directory, relative to the directory.'''
  result = []
  for dirpath, _, files in os.walk(path):
    for f in files:
      result.append(os.path.join(dirpath, f)[len(path) + 1:])
  return result


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('framework.gyp', chdir='framework')

  test.build('framework.gyp', 'test_framework', chdir='framework')

  # Binary
  test.built_file_must_exist(
      'Test Framework.framework/Versions/A/Test Framework',
      chdir='framework')

  # Info.plist
  info_plist = test.built_file_path(
      'Test Framework.framework/Versions/A/Resources/Info.plist',
      chdir='framework')
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'com.yourcompany.Test_Framework')

  # Resources
  test.built_file_must_exist(
      'Test Framework.framework/Versions/A/Resources/English.lproj/'
      'InfoPlist.strings',
      chdir='framework')

  # Symlinks created by packaging process
  test.built_file_must_exist('Test Framework.framework/Versions/Current',
                             chdir='framework')
  test.built_file_must_exist('Test Framework.framework/Resources',
                             chdir='framework')
  test.built_file_must_exist('Test Framework.framework/Test Framework',
                             chdir='framework')
  # PkgInfo.
  test.built_file_must_not_exist(
      'Test Framework.framework/Versions/A/Resources/PkgInfo',
      chdir='framework')

  # Check that no other files get added to the bundle.
  if set(ls(test.built_file_path('Test Framework.framework',
                                 chdir='framework'))) != \
     set(['Versions/A/Test Framework',
          'Versions/A/Resources/Info.plist',
          'Versions/A/Resources/English.lproj/InfoPlist.strings',
          'Test Framework',
          'Versions/A/Libraries/empty.c',  # Written by a gyp action.
          ]):
    test.fail_test()

  test.pass_test()
