#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test cases when multiple targets in different directories have the same name.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android', 'ninja', 'make'])

# xcode-ninja fails to generate a project due to id collisions
# cf. https://code.google.com/p/gyp/issues/detail?id=461
if test.format == 'xcode-ninja':
  test.skip_test()

test.run_gyp('subdirs.gyp', chdir='src')

test.relocate('src', 'relocate/src')

# Test that we build all targets.
test.build('subdirs.gyp', 'target', chdir='relocate/src')
test.must_exist('relocate/src/subdir1/action1.txt')
test.must_exist('relocate/src/subdir2/action2.txt')

# Test that we build all targets using the correct actions, even if they have
# the same names.
test.build('subdirs.gyp', 'target_same_action_name', chdir='relocate/src')
test.must_exist('relocate/src/subdir1/action.txt')
test.must_exist('relocate/src/subdir2/action.txt')

# Test that we build all targets using the correct rules, even if they have
# the same names.
test.build('subdirs.gyp', 'target_same_rule_name', chdir='relocate/src')
test.must_exist('relocate/src/subdir1/rule.txt')
test.must_exist('relocate/src/subdir2/rule.txt')

test.pass_test()
