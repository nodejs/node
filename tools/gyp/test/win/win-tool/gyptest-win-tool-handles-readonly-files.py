#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure overwriting read-only files works as expected (via win-tool).
"""

import TestGyp

import filecmp
import os
import stat
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  # First, create the source files.
  os.makedirs('subdir')
  read_only_files = ['read-only-file', 'subdir/A', 'subdir/B', 'subdir/C']
  for f in read_only_files:
    test.write(f, 'source_contents')
    test.chmod(f, stat.S_IREAD)
    if os.access(f, os.W_OK):
      test.fail_test()

  # Second, create the read-only destination files. Note that we are creating
  # them where the ninja and win-tool will try to copy them to, in order to test
  # that copies overwrite the files.
  os.makedirs(test.built_file_path('dest/subdir'))
  for f in read_only_files:
    f = os.path.join('dest', f)
    test.write(test.built_file_path(f), 'SHOULD BE OVERWRITTEN')
    test.chmod(test.built_file_path(f), stat.S_IREAD)
    # Ensure not writable.
    if os.access(test.built_file_path(f), os.W_OK):
      test.fail_test()

  test.run_gyp('copies_readonly_files.gyp')
  test.build('copies_readonly_files.gyp')

  # Check the destination files were overwritten by ninja.
  for f in read_only_files:
    f = os.path.join('dest', f)
    test.must_contain(test.built_file_path(f), 'source_contents')

  # This will fail if the files are not the same mode or contents.
  for f in read_only_files:
    if not filecmp.cmp(f, test.built_file_path(os.path.join('dest', f))):
      test.fail_test()

  test.pass_test()
