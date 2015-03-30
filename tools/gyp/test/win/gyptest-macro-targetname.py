#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure macro expansion of $(TargetName) and $(TargetDir) are handled.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'vs-macros'
  test.run_gyp('targetname.gyp', chdir=CHDIR)
  test.build('targetname.gyp', test.ALL, chdir=CHDIR)
  test.built_file_must_exist('test_targetname_plus_something1.exe',
          chdir=CHDIR)
  test.built_file_must_exist(
          'prod_prefixtest_targetname_with_prefix_plus_something2.exe',
          chdir=CHDIR)
  test.built_file_must_exist('prod_name_plus_something3.exe', chdir=CHDIR)
  test.built_file_must_exist('prod_prefixprod_name_plus_something4.exe',
          chdir=CHDIR)
  test.pass_test()
