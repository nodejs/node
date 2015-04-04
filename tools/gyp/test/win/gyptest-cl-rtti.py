#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure RTTI setting is extracted properly.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'compiler-flags'
  test.run_gyp('rtti.gyp', chdir=CHDIR)

  # Must fail.
  test.build('rtti.gyp', 'test_rtti_off', chdir=CHDIR, status=1)

  # Must succeed.
  test.build('rtti.gyp', 'test_rtti_on', chdir=CHDIR)

  # Must succeed.
  test.build('rtti.gyp', 'test_rtti_unset', chdir=CHDIR)

  test.pass_test()
