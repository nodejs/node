#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tests the use of rules with the same output file name.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('subdirs.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('subdirs.gyp', test.ALL, chdir='relocate/src')
test.must_exist('relocate/src/subdir1/rule.txt')
test.must_exist('relocate/src/subdir2/rule.txt')

test.pass_test()
