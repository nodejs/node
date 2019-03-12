#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that 'copies' with app bundles are handled correctly.
"""

from __future__ import print_function

import TestGyp

import os
import sys
import time

if sys.platform == 'darwin':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('framework.gyp', chdir='framework')

  test.build('framework.gyp', 'copy_target', chdir='framework')

  # Check that the copy succeeded.
  test.built_file_must_exist(
      'Test Framework.framework/foo/Dependency Bundle.framework',
      chdir='framework')
  test.built_file_must_exist(
      'Test Framework.framework/foo/Dependency Bundle.framework/Versions/A',
      chdir='framework')
  test.built_file_must_exist(
      'Test Framework.framework/Versions/A/Libraries/empty.c',
      chdir='framework')

  # Verify BUILT_FRAMEWORKS_DIR is set and working.
  test.build('framework.gyp', 'copy_embedded', chdir='framework')

  test.built_file_must_exist(
      'Embedded/Test Framework.framework', chdir='framework')

  # Check that rebuilding the target a few times works.
  dep_bundle = test.built_file_path('Dependency Bundle.framework', chdir='framework')
  mtime = os.path.getmtime(dep_bundle)
  atime = os.path.getatime(dep_bundle)
  for i in range(3):
    os.utime(dep_bundle, (atime + i * 1000, mtime + i * 1000))
    test.build('framework.gyp', 'copy_target', chdir='framework')


  # Check that actions ran.
  test.built_file_must_exist('action_file', chdir='framework')

  # Test that a copy with the "Code Sign on Copy" flag on succeeds.
  test.build('framework.gyp', 'copy_target_code_sign', chdir='framework')

  test.pass_test()
