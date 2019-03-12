#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that missing 'sources' files are treated as fatal errors when the
the generator flag 'msvs_error_on_missing_sources' is set.
"""

import TestGyp
import os
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'], workdir='workarea_all')

  # With the flag not set
  test.run_gyp('hello_missing.gyp')

  # With the flag explicitly set to 0
  try:
    os.environ['GYP_GENERATOR_FLAGS'] = 'msvs_error_on_missing_sources=0'
    test.run_gyp('hello_missing.gyp')
  finally:
    del os.environ['GYP_GENERATOR_FLAGS']

  # With the flag explicitly set to 1
  try:
    os.environ['GYP_GENERATOR_FLAGS'] = 'msvs_error_on_missing_sources=1'
    # Test to make sure GYP raises an exception (exit status 1). Since this will
    # also print a backtrace, ensure that TestGyp is not checking that stderr is
    # empty by specifying None, which means do not perform any checking.
    # Instead, stderr is checked below to ensure it contains the expected
    # output.
    test.run_gyp('hello_missing.gyp', status=1, stderr=None)
  finally:
    del os.environ['GYP_GENERATOR_FLAGS']
  test.must_contain_any_line(test.stderr(),
                            ["Missing input files:"])

  test.pass_test()
