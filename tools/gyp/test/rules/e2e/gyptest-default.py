#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple rules when using an explicit build target of 'all'.
"""

from __future__ import print_function

import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)


import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('actions.gyp',
             '-G', 'xcode_ninja_target_pattern=^pull_in_all_actions$',
             chdir='src')

test.relocate('src', 'relocate/src')

test.build('actions.gyp', chdir='relocate/src')

expect = """\
Hello from program.c
Hello from function1.in
Hello from function2.in
"""

if test.format == 'xcode':
  chdir = 'relocate/src/subdir1'
else:
  chdir = 'relocate/src'
test.run_built_executable('program', chdir=chdir, stdout=expect)

expect = """\
Hello from program.c
Hello from function3.in
"""

if test.format == 'xcode':
  chdir = 'relocate/src/subdir3'
else:
  chdir = 'relocate/src'
test.run_built_executable('program2', chdir=chdir, stdout=expect)

test.must_match('relocate/src/subdir2/file1.out', 'Hello from file1.in\n')
test.must_match('relocate/src/subdir2/file2.out', 'Hello from file2.in\n')

test.must_match('relocate/src/subdir2/file1.out2', 'Hello from file1.in\n')
test.must_match('relocate/src/subdir2/file2.out2', 'Hello from file2.in\n')

test.must_match('relocate/src/subdir2/file1.out4', 'Hello from file1.in\n')
test.must_match('relocate/src/subdir2/file2.out4', 'Hello from file2.in\n')
test.must_match('relocate/src/subdir2/file1.copy', 'Hello from file1.in\n')

test.must_match('relocate/src/external/file1.external_rules.out',
                'Hello from file1.in\n')
test.must_match('relocate/src/external/file2.external_rules.out',
                'Hello from file2.in\n')

test.pass_test()
