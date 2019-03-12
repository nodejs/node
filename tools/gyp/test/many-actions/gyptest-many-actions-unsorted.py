#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure lots of actions in the same target don't cause exceeding command
line length.
"""

from __future__ import print_function

import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('many-actions-unsorted.gyp')

test.build('many-actions-unsorted.gyp', test.ALL)
for i in range(15):
  test.built_file_must_exist('generated_%d.h' % i)

# Make sure the optimized cygwin setup doesn't cause problems for incremental
# builds.
test.touch('file1')
test.build('many-actions-unsorted.gyp', test.ALL)

test.touch('file0')
test.build('many-actions-unsorted.gyp', test.ALL)

test.touch('file2')
test.touch('file3')
test.touch('file4')
test.build('many-actions-unsorted.gyp', test.ALL)

test.pass_test()
