#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure we build and include .rc files.
"""

from __future__ import print_function

import TestGyp

import sys

if sys.platform == 'win32':
  print("This test is currently disabled: https://crbug.com/483696.")
  sys.exit(0)

  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'rc-build'
  test.run_gyp('hello.gyp', chdir=CHDIR)
  test.build('hello.gyp', test.ALL, chdir=CHDIR)
  test.up_to_date('hello.gyp', 'resource_only_dll', chdir=CHDIR)
  test.run_built_executable('with_resources', chdir=CHDIR, status=4)

  test.pass_test()
