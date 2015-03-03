#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure we include the default libs.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'linker-flags'
  test.run_gyp('no-default-libs.gyp', chdir=CHDIR)
  test.build('no-default-libs.gyp', test.ALL, chdir=CHDIR, status=1)

  test.pass_test()
