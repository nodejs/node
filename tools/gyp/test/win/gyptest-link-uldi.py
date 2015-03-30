#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure that when ULDI is on, we link .objs that make up .libs rather than
the .libs themselves.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'uldi'
  test.run_gyp('uldi.gyp', chdir=CHDIR)
  # When linking with ULDI, the duplicated function from the lib will be an
  # error.
  test.build('uldi.gyp', 'final_uldi', chdir=CHDIR, status=1)
  # And when in libs, the duplicated function will be silently dropped, so the
  # build succeeds.
  test.build('uldi.gyp', 'final_no_uldi', chdir=CHDIR)

  test.pass_test()
