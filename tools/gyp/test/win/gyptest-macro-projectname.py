#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure macro expansion of $(ProjectName) is handled.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'vs-macros'
  test.run_gyp('projectname.gyp', chdir=CHDIR)
  test.build('projectname.gyp', test.ALL, chdir=CHDIR)
  test.built_file_must_exist('test_expansions_plus_something.exe', chdir=CHDIR)
  test.built_file_must_exist(
      'test_with_product_name_plus_something.exe', chdir=CHDIR)
  test.pass_test()
