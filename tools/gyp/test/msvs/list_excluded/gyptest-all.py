#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that msvs_list_excluded_files=0 doesn't list files that would
normally be in _excluded_files, and that if that flag is not set, then they
are still listed.
"""

import os
import TestGyp

test = TestGyp.TestGyp(formats=['msvs'], workdir='workarea_all')


# with the flag set to 0
try:
  os.environ['GYP_GENERATOR_FLAGS'] = 'msvs_list_excluded_files=0'
  test.run_gyp('hello_exclude.gyp')
finally:
  del os.environ['GYP_GENERATOR_FLAGS']
if test.uses_msbuild:
  test.must_not_contain('hello.vcxproj', 'hello_mac')
else:
  test.must_not_contain('hello.vcproj', 'hello_mac')


# with the flag not set
test.run_gyp('hello_exclude.gyp')
if test.uses_msbuild:
  test.must_contain('hello.vcxproj', 'hello_mac')
else:
  test.must_contain('hello.vcproj', 'hello_mac')


# with the flag explicitly set to 1
try:
  os.environ['GYP_GENERATOR_FLAGS'] = 'msvs_list_excluded_files=1'
  test.run_gyp('hello_exclude.gyp')
finally:
  del os.environ['GYP_GENERATOR_FLAGS']
if test.uses_msbuild:
  test.must_contain('hello.vcxproj', 'hello_mac')
else:
  test.must_contain('hello.vcproj', 'hello_mac')


test.pass_test()
