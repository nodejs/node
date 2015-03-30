#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that msvs_external_builder being set will invoke the provided
msvs_external_builder_build_cmd and msvs_external_builder_clean_cmd, and will
not invoke MSBuild actions and rules.
"""

import os
import sys
import TestGyp

if int(os.environ.get('GYP_MSVS_VERSION', 0)) < 2010:
  sys.exit(0)

test = TestGyp.TestGyp(formats=['msvs'], workdir='workarea_all')

# without the flag set
test.run_gyp('external.gyp')
test.build('external.gyp', target='external')
test.must_not_exist('external_builder.out')
test.must_exist('msbuild_rule.out')
test.must_exist('msbuild_action.out')
test.must_match('msbuild_rule.out', 'msbuild_rule.py hello.z a b c')
test.must_match('msbuild_action.out', 'msbuild_action.py x y z')
os.remove('msbuild_rule.out')
os.remove('msbuild_action.out')

# with the flag set, using Build
try:
  os.environ['GYP_DEFINES'] = 'use_external_builder=1'
  test.run_gyp('external.gyp')
  test.build('external.gyp', target='external')
finally:
  del os.environ['GYP_DEFINES']
test.must_not_exist('msbuild_rule.out')
test.must_not_exist('msbuild_action.out')
test.must_exist('external_builder.out')
test.must_match('external_builder.out', 'external_builder.py build 1 2 3')
os.remove('external_builder.out')

# with the flag set, using Clean
try:
  os.environ['GYP_DEFINES'] = 'use_external_builder=1'
  test.run_gyp('external.gyp')
  test.build('external.gyp', target='external', clean=True)
finally:
  del os.environ['GYP_DEFINES']
test.must_not_exist('msbuild_rule.out')
test.must_not_exist('msbuild_action.out')
test.must_exist('external_builder.out')
test.must_match('external_builder.out', 'external_builder.py clean 4 5')
os.remove('external_builder.out')

test.pass_test()
