#!/usr/bin/env python

# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that excluded files are listed in sources_for_indexing.xcodeproj by
default, and that the generator flag xcode_ninja_list_excluded_files can be
used to override the default behavior.
"""

import os
import TestGyp


test = TestGyp.TestGyp()

if test.format != 'xcode-ninja':
  test.skip_test()


# With the generator flag not set.
test.run_gyp('hello_exclude.gyp')
test.must_contain(
  'sources_for_indexing.xcodeproj/project.pbxproj', 'hello_excluded.cpp')


# With the generator flag set to 0.
try:
  os.environ['GYP_GENERATOR_FLAGS'] = 'xcode_ninja_list_excluded_files=0'
  test.run_gyp('hello_exclude.gyp')
finally:
  del os.environ['GYP_GENERATOR_FLAGS']
test.must_not_contain(
  'sources_for_indexing.xcodeproj/project.pbxproj', 'hello_excluded.cpp')


# With the generator flag explicitly set to 1.
try:
  os.environ['GYP_GENERATOR_FLAGS'] = 'xcode_ninja_list_excluded_files=1'
  test.run_gyp('hello_exclude.gyp')
finally:
  del os.environ['GYP_GENERATOR_FLAGS']
test.must_contain(
  'sources_for_indexing.xcodeproj/project.pbxproj', 'hello_excluded.cpp')


test.pass_test()
