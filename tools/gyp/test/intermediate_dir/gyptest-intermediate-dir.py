#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that targets have independent INTERMEDIATE_DIRs.
"""

import TestGyp

import os
import sys

test = TestGyp.TestGyp()

test.run_gyp('test.gyp', chdir='src')
test.build('test.gyp', 'target1', chdir='src')
# Check stuff exists.
intermediate_file1 = test.read('src/outfile.txt')
test.must_contain(intermediate_file1, 'target1')

shared_intermediate_file1 = test.read('src/shared_outfile.txt')
test.must_contain(shared_intermediate_file1, 'shared_target1')

test.run_gyp('test2.gyp', chdir='src')
test.build('test2.gyp', 'target2', chdir='src')
# Check INTERMEDIATE_DIR file didn't get overwritten but SHARED_INTERMEDIAT_DIR
# file did.
intermediate_file2 = test.read('src/outfile.txt')
test.must_contain(intermediate_file1, 'target1')
test.must_contain(intermediate_file2, 'target2')

shared_intermediate_file2 = test.read('src/shared_outfile.txt')
if shared_intermediate_file1 != shared_intermediate_file2:
  test.fail_test(shared_intermediate_file1 + ' != ' + shared_intermediate_file2)

# These sometimes fail flakily with the xcode generator due to the shared file
# still containing 'target1'. Maybe the xcode build runs actions asynchronously?
# Asserting that the intermediate file names are identical is good enough.
#test.must_contain(shared_intermediate_file1, 'shared_target2')
#test.must_contain(shared_intermediate_file2, 'shared_target2')

test.pass_test()
