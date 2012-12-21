#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that objc settings are handled correctly.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  # set |match| to ignore build stderr output.
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'],
                         match = lambda a, b: True)

  CHDIR = 'objc-gc'
  test.run_gyp('test.gyp', chdir=CHDIR)

  build_error_code = {
    'xcode': [1, 65],  # Linker error code. 1 on Xcode 3, 65 on Xcode 4
    'make': 2,
    'ninja': 1,
  }[test.format]

  test.build('test.gyp', 'gc_exe_fails', chdir=CHDIR, status=build_error_code)
  test.build(
      'test.gyp', 'gc_off_exe_req_lib', chdir=CHDIR, status=build_error_code)

  test.build('test.gyp', 'gc_req_exe', chdir=CHDIR)
  test.run_built_executable('gc_req_exe', chdir=CHDIR, stdout="gc on: 1\n")

  test.build('test.gyp', 'gc_exe_req_lib', chdir=CHDIR)
  test.run_built_executable('gc_exe_req_lib', chdir=CHDIR, stdout="gc on: 1\n")

  test.build('test.gyp', 'gc_exe', chdir=CHDIR)
  test.run_built_executable('gc_exe', chdir=CHDIR, stdout="gc on: 1\n")

  test.build('test.gyp', 'gc_off_exe', chdir=CHDIR)
  test.run_built_executable('gc_off_exe', chdir=CHDIR, stdout="gc on: 0\n")

  test.pass_test()
